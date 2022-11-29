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

typedef enum {
	NR_RG0,
	NR_RG1,
	NR_REGS,
} confreg_regs_t;

static uint32_t regs[NR_REGS] = {
	[NR_RG0] = 0x0,
	[NR_RG1] = 0x0,
};

static uint32_t *confreg_base;
static SDL_Renderer *renderer = NULL;
void DrawCircle(SDL_Renderer * renderer, int32_t centreX, int32_t centreY, int32_t radius)
{
   const int32_t diameter = (radius * 2);

   int32_t x = (radius - 1);
   int32_t y = 0;
   int32_t tx = 1;
   int32_t ty = 1;
   int32_t error = (tx - diameter);

   while (x >= y)
   {
      //  Each of the following renders an octant of the circle
      SDL_RenderDrawPoint(renderer, centreX + x, centreY - y);
      SDL_RenderDrawPoint(renderer, centreX + x, centreY + y);
      SDL_RenderDrawPoint(renderer, centreX - x, centreY - y);
      SDL_RenderDrawPoint(renderer, centreX - x, centreY + y);
      SDL_RenderDrawPoint(renderer, centreX + y, centreY - x);
      SDL_RenderDrawPoint(renderer, centreX + y, centreY + x);
      SDL_RenderDrawPoint(renderer, centreX - y, centreY - x);
      SDL_RenderDrawPoint(renderer, centreX - y, centreY + x);

      if (error <= 0)
      {
	 ++y;
	 error += ty;
	 ty += 2;
      }

      if (error > 0)
      {
	 --x;
	 tx += 2;
	 error += (tx - diameter);
      }
   }
}

static void init_gpio() {
	SDL_Window *window = NULL;
	char title[128];
	sprintf(title, "%s-NEMU gpio simulation", str(__GUEST_ISA__));
	SDL_Init(SDL_INIT_EVERYTHING);              // Initialize SDL2

	// Create an application window with the following settings:
	window = SDL_CreateWindow(
		title,                  // window title
		SDL_WINDOWPOS_UNDEFINED,           // initial x position
		SDL_WINDOWPOS_UNDEFINED,           // initial y position
		640,                               // width, in pixels
		480,                               // height, in pixels
		SDL_WINDOW_SHOWN // flags - see below
	);
	renderer = SDL_CreateRenderer(window, -1, 0);
}

void update_gpio()
{
	// printf("update_gpio");
	// SDL_RenderClear(renderer);

	int r = 0, g = 0, b = 0;
	if ((regs[NR_RG0] & 0x3) == 0) {
		SDL_Rect rect = {
		.h = 20,
		.w = 20,
		.x = 0,
		.y = 0
		};
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_RenderDrawRect(renderer, &rect);
	} else {
		r = regs[NR_RG0] & 0x1 ? 255 : 0;
		g = (regs[NR_RG0] & 0x2) >> 1 ? 255 : 0;
		SDL_SetRenderDrawColor(renderer, r, g, b, 255);
		SDL_Rect rect = {
			.h = 20,
			.w = 20,
			.x = 0,
			.y = 0
		};
		SDL_RenderFillRect(renderer, &rect);
	}
	
	r = 0;
	g = 0;
	b = 0;
	if ((regs[NR_RG1] & 0x3) == 0) {
		SDL_Rect rect = {
		.h = 20,
		.w = 20,
		.x = 20,
		.y = 0
		};
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_RenderDrawRect(renderer, &rect);
	} else {
		r = regs[NR_RG1] & 0x1 ? 255 : 0;
		g = (regs[NR_RG1] & 0x2) >> 1 ? 255 : 0;
		SDL_SetRenderDrawColor(renderer, r, g, b, 255);
		SDL_Rect rect = {
			.h = 20,
			.w = 20,
			.x = 20,
			.y = 0
		};
		SDL_RenderFillRect(renderer, &rect);
	}

	SDL_RenderPresent(renderer);
}

static confreg_regs_t get_reg_idx(uint32_t offset, bool is_write)
{
	switch (offset)
	{
	case 0xf030:
		return NR_RG0;
	case 0xf040:
		return NR_RG1;
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
	case NR_RG0: case NR_RG1:
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
