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

#include "local-include/reg.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>

#define R(i) gpr(i)
#define Mr vaddr_read
#define Mw vaddr_write

enum {
	TYPE_1RI20,
	TYPE_2RI12,
	TYPE_N, // none
};

#define src(num, reg) do { *src##num = R(reg); } while(0)
#define si20() do { *imm = SEXT(BITS(i, 24, 5), 20) << 12; } while(0)
#define si12() do { *imm = SEXT(BITS(i, 21, 10), 12); } while(0)

static void decode_operand(Decode *s, int *dest, word_t *src1, word_t *src2, word_t *imm, int type) {
	uint32_t i = s->isa.inst.val;
	int rd  = BITS(i, 4, 0);
	int rj = BITS(i, 9, 5);
	// int rk = BITS(i, 14, 10);
	*dest = rd;
	switch (type) {
		case TYPE_1RI20:
			si20();
			break;
		case TYPE_2RI12:
			src(1, rj);
			src(2, rd);
			si12();
			break;
	}
}

static int decode_exec(Decode *s) {
	int dest = 0;
	word_t src1 = 0, src2 = 0, imm = 0;
	s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->isa.inst.val)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ )			\
{										\
	const char inst_name[] = #name;						\
	printf("%s\n", inst_name);						\
	decode_operand(s, &dest, &src1, &src2, &imm, concat(TYPE_, type));	\
	__VA_ARGS__ ;								\
}

	INSTPAT_START();
	/*	Inst Pattern				Inst Name	Inst Type	Inst Op */
	/* type 1RI20 */
	INSTPAT("0001010 ???????????????????? ?????",	lu12i.w,	1RI20,		R(dest) = imm);
	
	/* type 2RI12 */
	INSTPAT("0010100010 ???????????? ????? ?????",	ld.w,		2RI12,		R(dest) = Mr(src1 + imm, 4));
	INSTPAT("0010100110 ???????????? ????? ?????",	st.w,		2RI12,		Mw(src1 + imm, 4, src2));
	
	/* type None */
	INSTPAT("11111 000000000000000000000000000",	ntrap,		N,		NEMUTRAP(s->pc, R(4))); // R(4) is $a0
	INSTPAT("????????????????????????????????",	inv,		N,		INV(s->pc));
	INSTPAT_END();

	R(0) = 0; // reset $zero to 0

	return 0;
}

int isa_exec_once(Decode *s) {
	s->isa.inst.val = inst_fetch(&s->snpc, 4);
	return decode_exec(s);
}
