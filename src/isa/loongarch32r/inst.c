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

#define GR(name) (*((word_t *)(name)))
#define Memory_Load vaddr_read
#define Memory_Store vaddr_write

enum {
	TYPE_1RI20,
	TYPE_2RI12,
	TYPE_NTRAP,
	TYPE_N, // none
};

#define get_reg_ptr(name, base)					\
	do {							\
		int name##_idx = BITS(i, (base + 4), base);	\
		*name = &gpr(name##_idx);			\
	} while(0)
#define RJ() get_reg_ptr(rj, 5)
#define RK() get_reg_ptr(rk, 10)
#define RD() get_reg_ptr(rd, 0)
#define SI20() do { *imm = SEXT(BITS(i, 24, 5), 20) << 12; } while(0)
#define SI12() do { *imm = SEXT(BITS(i, 21, 10), 12); } while(0)

#define _2R() { RJ(); RD(); }
#define _1R() { RD(); }
#define _NR() { *rj = &gpr(4); }

static void decode_operand(Decode *s, word_t *imm, int type, word_t **rj, word_t **rk, word_t **rd) {
	uint32_t i = s->isa.inst.val;
	switch (type) {
		case TYPE_1RI20:
			_1R();
			SI20();
			break;
		case TYPE_2RI12:
			_2R();
			SI12();
			break;
		case TYPE_NTRAP:
			_NR();
			break;

	}
}

static int decode_exec(Decode *s) {
	word_t *rj = NULL;
	word_t *rk = NULL;
	word_t *rd = NULL;
	word_t imm = 0;
	s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->isa.inst.val)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ )			\
{										\
	const char inst_name[] = #name;						\
	printf("%s\n", inst_name);						\
	decode_operand(s, &imm, concat(TYPE_, type), &rj, &rk, &rd);		\
	__VA_ARGS__ ;								\
}

	INSTPAT_START();
	/*	Inst Pattern				Inst Name	Inst Type	Inst Op */
	/* type 1RI20 */
	INSTPAT("0001010 ???????????????????? ?????",	lu12i.w,	1RI20,		GR(rd) = imm);
	
	/* type 2RI12 */
	INSTPAT("0010100010 ???????????? ????? ?????",	ld.w,		2RI12,		GR(rd) = Memory_Load(GR(rj) + imm, 4));
	INSTPAT("0010100110 ???????????? ????? ?????",	st.w,		2RI12,		Memory_Store(GR(rj) + imm, 4, GR(rd)));
	
	/* type None */
	INSTPAT("11111 000000000000000000000000000",	ntrap,		NTRAP,		NEMUTRAP(s->pc, GR(rj)));
	INSTPAT("????????????????????????????????",	inv,		N,		INV(s->pc));
	INSTPAT_END();

	gpr(0) = 0; // reset $zero to 0

	return 0;
}

int isa_exec_once(Decode *s) {
	s->isa.inst.val = inst_fetch(&s->snpc, 4);
	return decode_exec(s);
}
