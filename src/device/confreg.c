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

static uint32_t *confreg_base;
static SDL_Renderer *renderer = NULL;

static void init_gpio() {
  SDL_Window *window = NULL;
  char title[128];
  sprintf(title, "%s-NEMU gpio simulation", str(__GUEST_ISA__));
  SDL_Init(SDL_INIT_VIDEO);
  SDL_CreateWindowAndRenderer(
      800,
      600,
      0, &window, &renderer);
  SDL_SetWindowTitle(window, title);
}

static void confreg_io_handler(uint32_t offset, int len, bool is_write)
{

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
