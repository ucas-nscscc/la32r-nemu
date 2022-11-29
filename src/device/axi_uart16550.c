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

#include <isa.h>
#include <utils.h>
#include <device/map.h>

#include <sys/select.h>
#include <unistd.h>

/* https://docs.xilinx.com/v/u/en-US/pg143-axi-uart16550 */
// NOTE: this is compatible to AXI UART 16550

#define COM_RX		(char *)0x1000	// In:  Receive buffer (DLAB=0)
#define COM_TX		(char *)0x1000	// Out: Transmit buffer (DLAB=0)
#define COM_DLL		(char *)0x1000	// Out: Divisor Latch Low (DLAB=1)
#define COM_DLM		(char *)0x1004	// Out: Divisor Latch High (DLAB=1)
#define COM_IER		(char *)0x1004	// Out: Interrupt Enable Register
#define COM_IER_RDI	0x01	// Enable receiver data interrupt
#define COM_IIR		(char *)0x1008	// In:  Interrupt ID Register
#define COM_FCR		(char *)0x1008	// Out: FIFO Control Register
#define COM_FCR_RST	0x06	// FCR: disable FIFO and reset
#define COM_LCR		(char *)0x100C	// Out: Line Control Register
#define COM_LCR_DLAB	0x80	// Divisor latch access bit
#define COM_LCR_WLEN8	0x03	// Wordlength: 8 bits
#define COM_MCR		(char *)0x1010	// Out: Modem Control Register
#define COM_MCR_RTS	0x02	// RTS complement
#define COM_MCR_DTR	0x01	// DTR complement
#define COM_MCR_OUT2	0x08	// Out2 complement
#define COM_LSR		(char *)0x1014	// In:  Line Status Register
#define COM_LSR_DATA	0x01	// Data available
#define COM_LSR_TXRDY	0x20	// Transmit buffer avail
#define COM_LSR_TSRE	0x40	// Transmitter off

typedef enum {
	NR_RBR,
	NR_THR,
	NR_IER,
	NR_IIR,
	NR_FCR,
	NR_LCR,
	NR_MCR,
	NR_LSR,
	NR_MSR,
	NR_SCR,
	NR_DLL,
	NR_DLM,
	NR_REGS,
} uart_regs_t;

#define fetch_bit(reg, bit) BITS(regs[NR_##reg], bit, bit)

static uint32_t regs[NR_REGS] = {
	[NR_IER] = 0x0,
	[NR_LCR] = 0x0,
	[NR_LSR] = COM_LSR_TXRDY,
};

static uint32_t *uart_base;

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

void send_uart(int c)
{
	regs[NR_RBR] = (char)c;
	regs[NR_LSR] = regs[NR_LSR] | COM_LSR_DATA;
	extern void dev_raise_intr(word_t);
	if (regs[NR_IER] & COM_IER_RDI) {
		dev_raise_intr(CONFIG_UART_INTR);
	}
}

static void uart_putc(char ch)
{
	MUXDEF(CONFIG_TARGET_AM, putch(ch), putc(ch, stderr));
}

void update_uart()
{
	if (kbhit()) {
		int c = getch();
		send_uart(c);
	}
	if (!(regs[NR_LSR] & COM_LSR_TXRDY)) {
		uart_putc(regs[NR_THR] & 0xff);
		regs[NR_LSR] = regs[NR_LSR] | COM_LSR_TXRDY;
	}
}

static uart_regs_t get_reg_idx(uint32_t offset, bool is_write)
{
	switch (offset)
	{
	case 0x1000:
		return fetch_bit(LCR, 7) == 0 ? (is_write ? NR_THR : NR_RBR) : NR_DLL;
	case 0x1004:
		return fetch_bit(LCR, 7) == 0 ? NR_IER : NR_DLM;
	case 0x1008:
		return (fetch_bit(LCR, 7) == 1 && !is_write) ? NR_FCR : is_write ? NR_FCR : NR_IIR;
	case 0x100c:
		return NR_LCR;
	case 0x1010:
		return NR_MSR;
	case 0x1014:
		return NR_LSR;
	case 0x1018:
		return NR_MSR;
	case 0x101c:
		return NR_SCR;
	default:
		return NR_REGS;
	}
}

static void uart_io_handler(uint32_t offset, int len, bool is_write)
{
	assert(len == 4);
	uart_regs_t reg_idx = get_reg_idx(offset, is_write);
	assert (reg_idx < NR_REGS);
	if (is_write) {
		regs[reg_idx] = uart_base[offset / 4];
	} else {
		uart_base[offset / 4] = regs[reg_idx];
	}
	switch (reg_idx)
	{
	case NR_RBR:
		regs[NR_LSR] = regs[NR_LSR] & ~(COM_LSR_DATA);
		extern void dev_clear_intr(word_t);
		dev_clear_intr(CONFIG_UART_INTR);
		break;
	case NR_THR:
		regs[NR_LSR] = regs[NR_LSR] & ~(COM_LSR_TXRDY);
		break;
	case NR_IER:
	case NR_FCR:
	case NR_LCR:
	case NR_LSR:
	case NR_DLL:
	case NR_DLM:
		break;
	default:
		panic("uart do not support offset = %d", offset);
		break;
	}
}

void init_uart()
{
	uart_base = (uint32_t *)new_space(64 * 1024);
#ifdef CONFIG_HAS_PORT_IO
	add_pio_map ("uart", CONFIG_UART_PORT, uart_base, 32, uart_io_handler);
#else
	add_mmio_map("uart", CONFIG_UART_MMIO, uart_base, 64 * 1024, uart_io_handler);
#endif

}
