/**
 * @file button.c
 * @author wenshuyu (wsy2161826815@163.com)
 * @brief 按键组件
 * @version 1.0
 * @date 2024-08-12
 * 
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 */

#include "button.h"

#define NULL ((void *)0)

typedef enum {
	BTN_IO_EVENT_UP, //按键IO按下事件
	BTN_IO_EVENT_DOWN, //按键IO弹起事件
} BTN_IO_EVENT_E;

/*
 *函数指针类型
 *在给button_event_e (*)(button_t *p_btn, BTN_IO_EVENT_E io_ev)取别名,  之后可以用on_state_handler xxx 来作为函数的参数
 */
typedef button_event_e (*on_state_handler)(button_t *p_btn, BTN_IO_EVENT_E io_ev);

static button_event_e on_up_handler(button_t *p_btn, BTN_IO_EVENT_E io_ev); //弹起
static button_event_e on_down_handler(button_t *p_btn, BTN_IO_EVENT_E io_ev); //按下
static button_event_e on_up_suspense_handler(button_t *p_btn, BTN_IO_EVENT_E io_ev); //弹起去抖阶段?不是很清楚
static button_event_e on_down_short_handler(button_t *p_btn, BTN_IO_EVENT_E io_ev); //短按
static button_event_e on_down_long_handler(button_t *p_btn, BTN_IO_EVENT_E io_ev); //长按

/*
 *（inline expansion）是编译器优化中的一个概念，指的是编译器在处理函数调用时，不是生成跳转到函数代码的指令，
 *而是将函数的代码直接复制到调用点的位置。这样做的目的是为了减少函数调用的开销，包括参数传递、返回值处理、栈帧的创建和销毁等
 *功能:判断点击的类型,是单击,双击,还是多次
 */
static inline button_event_e dispatch_click_type(uint32_t click_cnt)
{
	static const button_event_e click_type[3] = {
		BTN_EVENT_NONE,
		BTN_EVENT_SINGLE_CLICK,
		BTN_EVENT_DOUBLE_CLICK,
	};

	return (click_cnt < 3) ? click_type[click_cnt] : BTN_EVENT_MORE_CLICK;
}

/*
 *处理按钮的空闲（idle）状态时的事件
 *检查按键是否被释放
 */
static button_event_e on_idle_handler(button_t *p_btn, BTN_IO_EVENT_E io_ev)
{
	if (io_ev == BTN_IO_EVENT_UP)
		p_btn->state.state = on_up_handler;
	else {
		p_btn->state.counter = 0;
		p_btn->state.click_cnt = 1;
		p_btn->state.state = on_down_handler;
	}

	return BTN_EVENT_NONE;
}

/*用于处理按钮从按下状态变为释放状态（即按钮被释放后）的事件*/
static button_event_e on_up_handler(button_t *p_btn, BTN_IO_EVENT_E io_ev)
{
	if (io_ev == BTN_IO_EVENT_DOWN) {
		p_btn->state.counter = 0;
		p_btn->state.click_cnt = 1;
		p_btn->state.state = on_down_handler;
	}

	return BTN_EVENT_NONE;
}

/*用于处理按键释放之后的一个中间状态*/
static button_event_e on_up_suspense_handler(button_t *p_btn, BTN_IO_EVENT_E io_ev)
{
	button_event_e ev = BTN_EVENT_NONE;

	if (io_ev == BTN_IO_EVENT_UP) {
		if (++p_btn->state.counter >= p_btn->cfg.up_max_cnt) {
			p_btn->state.counter = 0;
			ev = dispatch_click_type(p_btn->state.click_cnt);
			p_btn->state.state = on_up_handler;
		}
	} else {
		p_btn->state.counter = 0;					 //清除计数
		++p_btn->state.click_cnt;                    //增加点击次数
		p_btn->state.state = on_down_short_handler;
	}

	return ev;
}

/*处理按键按下事件,如果counter大于长按的设定值判断为长按*/
static button_event_e on_down_handler(button_t *p_btn, BTN_IO_EVENT_E io_ev)
{
	button_event_e ev = BTN_EVENT_NONE;

	if (io_ev == BTN_IO_EVENT_UP) {
		ev = BTN_EVENT_POPUP;
		p_btn->state.counter = 0;
		p_btn->state.state = on_up_suspense_handler;
	} else {
		if (++p_btn->state.counter >= p_btn->cfg.long_min_cnt) {
			ev = BTN_EVENT_LONG_CLICK;
			p_btn->state.counter = 0;
			p_btn->state.state = on_down_long_handler;
		}
	}

	return ev;
}

static button_event_e on_down_short_handler(button_t *p_btn, BTN_IO_EVENT_E io_ev)
{
	button_event_e ev = BTN_EVENT_NONE;

	if (io_ev == BTN_IO_EVENT_UP) {
		ev = BTN_EVENT_POPUP;
		p_btn->state.counter = 0;
		p_btn->state.state = on_up_suspense_handler;
	} else {
		if (++p_btn->state.counter >= p_btn->cfg.up_max_cnt) {
			p_btn->state.counter = 0;
			ev = dispatch_click_type(p_btn->state.click_cnt);
			p_btn->state.state = on_down_long_handler;
		}
	}

	return ev;
}

static button_event_e on_down_long_handler(button_t *p_btn, BTN_IO_EVENT_E io_ev)
{
	button_event_e ev = BTN_EVENT_NONE;

	if (io_ev == BTN_IO_EVENT_UP) {
		ev = BTN_EVENT_POPUP;
		p_btn->state.state = on_up_handler;
	}
	return ev;
}

/*按键消抖*/
static inline uint8_t button_debounce(button_t *p_btn)
{
	uint8_t cur_lv = p_btn->cfg.f_io_read();  //读取当前电平

	p_btn->state.jit.asserted |= (p_btn->state.jit.previous & cur_lv);//前后两次电平一致
	p_btn->state.jit.asserted &= (p_btn->state.jit.previous | cur_lv);//前后两次电平不一致
	// 0 1   p_btn->state.jit.asserted = 0
	// 1 1   p_btn->state.jit.asserted = 1
	// 0 0   p_btn->state.jit.asserted = 0
	// 1 0   p_btn->state.jit.asserted = 0

	p_btn->state.jit.previous = cur_lv;

	return p_btn->state.jit.asserted;
}

//按键扫描
button_event_e button_scan(button_t *p_btn)
{
	uint8_t cur_level;
	button_ev_t ev;

	if (!p_btn || !p_btn->cfg.f_io_read || !p_btn->state.state)
		return BTN_EVENT_NONE;

	cur_level = button_debounce(p_btn);

	if (cur_level == p_btn->cfg.active_lv)
		ev.ev_type = ((on_state_handler)p_btn->state.state)(p_btn, BTN_IO_EVENT_DOWN);
	else
		ev.ev_type = ((on_state_handler)p_btn->state.state)(p_btn, BTN_IO_EVENT_UP);

	if (ev.ev_type != BTN_EVENT_NONE && p_btn->f_ev_cb) {
		ev.clicks = p_btn->state.click_cnt;
		p_btn->f_ev_cb(&ev);
	}

	return ev.ev_type;
}

button_t button_ctor(button_cfg_t *p_cfg, btn_event_callback cb)
{
	button_t btn;

	if (p_cfg) {
		btn.cfg = *p_cfg;
		btn.f_ev_cb = cb;
		btn.state.jit.previous = (p_cfg->active_lv ^ BUTTON_LEVEL_HIGH);
		btn.state.jit.asserted = btn.state.jit.previous;
		btn.state.state = on_idle_handler;
	} else {
		btn.cfg.f_io_read = NULL;
		btn.state.state = NULL;
	}

	return btn;
}
