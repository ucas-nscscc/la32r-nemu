/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
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
#include <utils.h>
#include <device/alarm.h>
#ifndef CONFIG_TARGET_AM
#include <SDL2/SDL.h>
#endif

#include <unistd.h>
#include <sys/select.h>

void init_map();
void init_serial();
void init_uart();
void init_timer();
void init_vga();
void init_i8042();
void init_audio();
void init_disk();
void init_sdcard();
void init_alarm();

void send_key(uint8_t, bool);
void send_uart(int);
void vga_update_screen();

/* refer to https://stackoverflow.com/questions/448944/c-non-blocking-keyboard-input */
static int kbhit()
{
	struct timeval tv = { 0L, 0L };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static int getch()
{
	int r;
	unsigned char c;
	if ((r = read(STDIN_FILENO, &c, sizeof(c))) < 0) {
		return r;
	} else {
		return c;
	}
}

void device_update() {
	static uint64_t last = 0;
	uint64_t now = get_time();
	if (now - last < 1000000 / TIMER_HZ) {
		return;
	}
	last = now;

	IFDEF(CONFIG_HAS_VGA, vga_update_screen());

#ifndef CONFIG_TARGET_AM
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				nemu_state.state = NEMU_QUIT;
				break;
#ifdef CONFIG_HAS_KEYBOARD
			// If a key was pressed
			case SDL_KEYDOWN:
			case SDL_KEYUP: {
				uint8_t k = event.key.keysym.scancode;
				bool is_keydown = (event.key.type == SDL_KEYDOWN);
				send_key(k, is_keydown);
				break;
			}
#endif
			default: break;
		}
	}
#endif
#ifdef CONFIG_HAS_UART
	if (kbhit()) {
		int c = getch();
		send_uart(c);
	}
#endif
}

void sdl_clear_event_queue() {
#ifndef CONFIG_TARGET_AM
	SDL_Event event;
	while (SDL_PollEvent(&event));
#endif
}

void init_device() {
	IFDEF(CONFIG_TARGET_AM, ioe_init());
	init_map();

	IFDEF(CONFIG_HAS_SERIAL, init_serial());
	IFDEF(CONFIG_HAS_UART, init_uart());
	IFDEF(CONFIG_HAS_TIMER, init_timer());
	IFDEF(CONFIG_HAS_VGA, init_vga());
	IFDEF(CONFIG_HAS_KEYBOARD, init_i8042());
	IFDEF(CONFIG_HAS_AUDIO, init_audio());
	IFDEF(CONFIG_HAS_DISK, init_disk());
	IFDEF(CONFIG_HAS_SDCARD, init_sdcard());

	IFNDEF(CONFIG_TARGET_AM, init_alarm());
}
