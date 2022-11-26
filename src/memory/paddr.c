/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
* Copyright (c) 2022 MiaoHao, the University of Chinese Academy of Science
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

#include <memory/host.h>
#include <memory/paddr.h>
#include <device/mmio.h>
#include <isa.h>

#if   defined(CONFIG_PMEM_MALLOC)
static uint8_t *pmem = NULL;
#else // CONFIG_PMEM_GARRAY
static uint8_t pmem[CONFIG_MSIZE] PG_ALIGN = {};
#endif

uint8_t* guest_to_host(paddr_t paddr) { return pmem + paddr - CONFIG_MBASE; }
paddr_t host_to_guest(uint8_t *haddr) { return haddr - pmem + CONFIG_MBASE; }

static word_t pmem_read(paddr_t addr, int len) {
	word_t ret = host_read(guest_to_host(addr), len);
	return ret;
}

static void pmem_write(paddr_t addr, int len, word_t data) {
	host_write(guest_to_host(addr), len, data);
}

static void out_of_bound(paddr_t addr) {
	panic("address = " FMT_PADDR " is out of bound of pmem [" FMT_PADDR ", " FMT_PADDR "] at pc = " FMT_WORD,
			addr, PMEM_LEFT, PMEM_RIGHT, cpu.pc);
}

void init_mem() {
#if   defined(CONFIG_PMEM_MALLOC)
	pmem = malloc(CONFIG_MSIZE);
	assert(pmem);
#endif
#ifdef CONFIG_MEM_RANDOM
	uint32_t *p = (uint32_t *)pmem;
	int i;
	for (i = 0; i < (int) (CONFIG_MSIZE / sizeof(p[0])); i ++) {
		p[i] = rand();
	}
#endif
	Log("physical memory area [" FMT_PADDR ", " FMT_PADDR "]", PMEM_LEFT, PMEM_RIGHT);
}

word_t paddr_read(paddr_t addr, int len) {
#ifdef CONFIG_MTRACE
	word_t rdata;
	bool in_mem = false;
	bool in_mmio = false;
	if (likely(in_mem = in_pmem(addr)))
		rdata = pmem_read(addr, len);
	else {
		IFDEF(CONFIG_DEVICE, rdata = mmio_read(addr, len); in_mmio = true);
	}
	log_write("[%s] addr: "FMT_PADDR", rdata: "FMT_WORD", len: 0x%x\n", in_mem ? "in mem read" : in_mmio ? "mmio read" : "out of bound read", addr, rdata, len);
	if (in_mem || in_mmio) {
		return rdata;
	}
#else
	if (likely(in_pmem(addr))) return pmem_read(addr, len);
	IFDEF(CONFIG_DEVICE, return mmio_read(addr, len));
#endif
	out_of_bound(addr);
	return 0;
}

void paddr_write(paddr_t addr, int len, word_t data) {
#ifdef CONFIG_MTRACE
	bool in_mem = false;
	bool in_mmio = false;
	if (likely(in_mem = in_pmem(addr))) 
		pmem_write(addr, len, data);
	else {
		IFDEF(CONFIG_DEVICE, mmio_write(addr, len, data); in_mmio = true);
	}
	log_write("[%s] addr: "FMT_PADDR", wdata: "FMT_WORD", len: 0x%x\n", in_mem ? "in mem write" : in_mmio ? "mmio write" : "out of bound write", addr, data, len);
	if (in_mem || in_mmio) {
		return;
	}
#else
	if (likely(in_pmem(addr))) { pmem_write(addr, len, data); return; }
	IFDEF(CONFIG_DEVICE, mmio_write(addr, len, data); return);
#endif
	out_of_bound(addr);
}
