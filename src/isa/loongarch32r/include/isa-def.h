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

#ifndef __ISA_LOONGARCH32R_H__
#define __ISA_LOONGARCH32R_H__

#include <common.h>

enum {
	CSR_CRMD,
	CSR_PRMD,
	CSR_EUEN,
	CSR_ECFG=0x4,
	CSR_ESTAT,
	CSR_ERA,
	CSR_BADV,
	CSR_EENTRY=0xc,
	CSR_TLBIDX=0x10,
	CSR_TLBEHI,
	CSR_TLBELO0,
	CSR_TLBELO1,
	CSR_ASID=0x18,
	CSR_PGDL,
	CSR_PGDH,
	CSR_PGD,
	CSR_CPUID=0x20,
	CSR_SAVE0=0x30,
	CSR_SAVE1,
	CSR_SAVE2,
	CSR_SAVE3,
	CSR_TID=0x40,
	CSR_TCFG,
	CSR_TVAL,
	CSR_TICLR,
	CSR_LLBCTL=0x60,
	CSR_TLBRENTRY=0x88,
	CSR_CTAG=0x98,
	CSR_DMW0=0x180,
	CSR_DMW1,
};

typedef struct {
	word_t gpr[32];
	vaddr_t pc;
	word_t csr[0x182];
} loongarch32r_CPU_state;

// decode
typedef struct {
	union {
		uint32_t val;
	} inst;
} loongarch32r_ISADecodeInfo;

#define isa_mmu_check(vaddr, len, type) (MMU_DIRECT)

#endif
