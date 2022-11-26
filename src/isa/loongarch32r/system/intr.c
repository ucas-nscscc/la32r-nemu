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

word_t isa_raise_intr(word_t NO, vaddr_t epc) {
	csr(CSR_PRMD) = (csr(CSR_CRMD) & 0x7) | (csr(CSR_PRMD) & ~(0x7));
	csr(CSR_CRMD) &= ~(0x7);
	csr(CSR_ERA) = epc;
	return csr(CSR_EENTRY);
}

word_t isa_query_intr() {
	word_t ESTAT_IS = csr(CSR_ESTAT) & 0x1fff;
	word_t CRMD_IE = csr(CSR_CRMD) & 0x4;
	word_t i = INTR_EMPTY;
	if (ESTAT_IS != 0 && CRMD_IE != 0) {
		i = 0;
		while ((ESTAT_IS & 0x1) == 0) {
			i++;
			ESTAT_IS >>=0x1;
		}
	}
	return i;
}
