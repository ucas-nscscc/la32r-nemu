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
#include "../local-include/reg.h"
#include "isa-def.h"

// NOTE: for loongarch, *_intr is exactly *_exception
word_t isa_raise_intr(word_t NO, vaddr_t epc) {
	csr(CSR_PRMD) = (csr(CSR_CRMD) & 0x7) | (csr(CSR_PRMD) & ~(0x7));
	csr(CSR_CRMD) &= ~(0x7);
	csr(CSR_ESTAT) = ((cpu.ecode & 0x3f) << 16) | (csr(CSR_ESTAT) & ~(0x3f << 16));
	csr(CSR_ESTAT) = ((cpu.esubcode & 0x1ff) << 22) | (csr(CSR_ESTAT) & ~(0x1ff << 22));
	csr(CSR_ERA) = epc;
	cpu.ex_taken = 0;
	return csr(CSR_EENTRY);
}

word_t isa_query_intr() {
	return cpu.ex_taken ? 1 : -1;
}
