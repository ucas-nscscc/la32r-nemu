/***************************************************************************************
* Copyright (c) 2022 MiaoHao, University of Chinese Academy of Science
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <common.h>
#include <device/map.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>

#define TIMER_FREQ CONFIG_CONFREG_TIMER_FREQ MHz
#define update_rg(rg_num)					\
do {								\
	int r = 0, g = 0;					\
	SDL_Rect rect;						\
	g = regs[NR_RG##rg_num] & 0x1 ? 255 : 0;		\
	r = (regs[NR_RG##rg_num] & 0x2) >> 1 ? 255 : 0;		\
	SDL_SetRenderDrawColor(renderer, r, g, 0, 255);		\
	rect.h = 20;						\
	rect.w = 20;						\
	rect.x = 20 * rg_num;					\
	rect.y = 0;						\
	SDL_RenderFillRect(renderer, &rect);			\
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);	\
	SDL_RenderDrawRect(renderer, &rect);			\
} while(0)

typedef enum {
	NR_RG0,
	NR_RG1,
	NR_TIMER,
	NR_REGS,
} confreg_regs_t;

static uint32_t regs[NR_REGS] = {
	[NR_RG0] = 0x0,
	[NR_RG1] = 0x0,
	[NR_TIMER] = 0x0,
};

static uint32_t *confreg_base;
static uint32_t old_us;
static uint32_t nr_us = TIMER_FREQ / (1 MHz);
static SDL_Renderer *renderer = NULL;

static void init_gpio() {
	SDL_Window *window = NULL;
	char title[128];

	sprintf(title, "%s-NEMU gpio simulation", str(__GUEST_ISA__));
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_CreateWindowAndRenderer(640, 480, 0, &window, &renderer);
	SDL_SetWindowTitle(window, title);

	old_us = get_time();
}

static void update_timer()
{
	uint32_t new_us = get_time();
	uint32_t us_interval = new_us - old_us;
	old_us = new_us;
	regs[NR_TIMER] += us_interval * nr_us;
}

void update_gpio()
{
	update_rg(0);
	update_rg(1);

	SDL_RenderPresent(renderer);

	update_timer();
}

static confreg_regs_t get_reg_idx(uint32_t offset, bool is_write)
{
	switch (offset)
	{
	case 0xf030:
		return NR_RG0;
	case 0xf040:
		return NR_RG1;
	case 0xe000:
		return NR_TIMER;
	default:
		return NR_REGS;
	}
}

static void confreg_io_handler(uint32_t offset, int len, bool is_write)
{
	assert(len == 4);
	confreg_regs_t reg_idx = get_reg_idx(offset, is_write);
	// assert (reg_idx < NR_REGS);
	if (is_write) {
		regs[reg_idx] = confreg_base[offset / 4];
	} else {
		confreg_base[offset / 4] = regs[reg_idx];
	}
	switch (reg_idx)
	{
	case NR_RG0: case NR_RG1: case NR_TIMER:
		break;
	default:
		// panic("uart do not support offset = %d", offset);
		break;
	}
}

void init_confreg() {
	confreg_base = (uint32_t *)new_space(64 * 1024);
#ifdef CONFIG_HAS_PORT_IO
	add_pio_map ("uart", CONFIG_UART_PORT, uart_base, 32, uart_io_handler);
#else
	add_mmio_map("confreg", CONFIG_CONFREG_MMIO, confreg_base, 64 * 1024, confreg_io_handler);
#endif
	init_gpio();
}
