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

#define GR(name) gpr(name)
#define Memory_Load vaddr_read
#define Memory_Store vaddr_write

enum {
	TYPE_3R,
	TYPE_2RUI5,
	TYPE_2RSI12,
	TYPE_2RUI12,
	TYPE_2RI16,
	TYPE_1RSI20,
	TYPE_I26,
	TYPE_NTRAP,
	TYPE_N, // none
};

#define get_reg_idx(ptr, base)				\
({							\
	int __base = base;				\
	int __idx = BITS(i, (__base + 4), __base);	\
	*ptr = __idx;					\
})
#define RJ() get_reg_idx(rj, 5)
#define RK() get_reg_idx(rk, 10)
#define RD() get_reg_idx(rd, 0)
#define SI20() do { *imm = SEXT(BITS(i, 24, 5), 20) << 12; } while(0)
#define SI12() do { *imm = SEXT(BITS(i, 21, 10), 12); } while(0)
#define UI12() do { *imm = BITS(i, 21, 10); } while(0)
#define UI5() do { *imm = BITS(i, 14, 10); } while(0)
#define I16() do { *imm = SEXT((BITS(i, 25, 10) << 2), 18); } while(0)
#define I26() do { *imm = SEXT((((BITS(i, 9, 0) << 16) | BITS(i, 25, 10)) << 2), 28); } while(0)

#define _3R() { RJ(); RK(); RD(); }
#define _2R() { RJ(); RD(); }
#define _1R() { RD(); }

static void decode_operand(Decode *s, int type, int *rj, int *rk, int *rd, word_t *imm) {
	uint32_t i = s->isa.inst.val;
	switch (type) {
		case TYPE_3R:
			_3R();
			break;
		case TYPE_2RUI5:
			_2R();
			UI5();
			break;
		case TYPE_2RSI12:
			_2R();
			SI12();
			break;
		case TYPE_2RUI12:
			_2R();
			UI12();
			break;
		case TYPE_2RI16:
			_2R();
			I16();
			break;
		case TYPE_1RSI20:
			_1R();
			SI20();
			break;
		case TYPE_I26:
			I26();
			break;
		case TYPE_NTRAP:
			_1R();
			break;
	}
}

static int decode_exec(Decode *s) {
	int rj = 0;
	int rk = 0;
	int rd = 0;
	word_t imm = 0;
	char instr[32];
	s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->isa.inst.val)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ )			\
{										\
	const char inst_name[] = #name;						\
	strcpy(instr, inst_name);						\
	decode_operand(s, concat(TYPE_, type), &rj, &rk, &rd, &imm);		\
	__VA_ARGS__ ;								\
}

	INSTPAT_START();
	/*	Inst Pattern				Inst Name	Inst Type	Inst Op */
	/* type 3R */
	INSTPAT("00000000000100000 ????? ????? ?????",	add.w,		3R,		uint64_t tmp = ((int64_t)(int32_t)GR(rj)) + ((int64_t)(int32_t)GR(rk)); GR(rd) = tmp & 0xffffffff);
	INSTPAT("00000000000100010 ????? ????? ?????",	sub.w,		3R,		uint64_t tmp = ((int64_t)(int32_t)GR(rj)) - ((int64_t)(int32_t)GR(rk)); GR(rd) = tmp & 0xffffffff);
	INSTPAT("00000000000100100 ????? ????? ?????",	slt.w,		3R,		GR(rd) = (signed)GR(rj) < (signed)GR(rk) ? 1 : 0);
	INSTPAT("00000000000100101 ????? ????? ?????",	sltu.w,		3R,		GR(rd) = (unsigned)GR(rj) < (unsigned)GR(rk) ? 1 : 0);
	INSTPAT("00000000000101000 ????? ????? ?????",	nor.w,		3R,		GR(rd) = ~(GR(rj) | GR(rk)));
	INSTPAT("00000000000101001 ????? ????? ?????",	and.w,		3R,		GR(rd) = GR(rj) & GR(rk));
	INSTPAT("00000000000101010 ????? ????? ?????",	or.w,		3R,		GR(rd) = GR(rj) | GR(rk));
	INSTPAT("00000000000101011 ????? ????? ?????",	xor.w,		3R,		GR(rd) = GR(rj) ^ GR(rk));
	INSTPAT("00000000000101110 ????? ????? ?????",	sll.w,		3R,		GR(rd) = GR(rj) << (GR(rk) & 0x1f));
	INSTPAT("00000000000101111 ????? ????? ?????",	srl.w,		3R,		GR(rd) = ((unsigned)GR(rj)) >> (GR(rk) & 0x1f));
	INSTPAT("00000000000110000 ????? ????? ?????",	sra.w,		3R,		GR(rd) = ((signed)GR(rj)) >> (GR(rk) & 0x1f));
	INSTPAT("00000000000111000 ????? ????? ?????",	mul.w,		3R,		int64_t product = ((int64_t)(int32_t)GR(rj)) * ((int64_t)(int32_t)GR(rk)); GR(rd) = product & 0xffffffff);
	INSTPAT("00000000000111001 ????? ????? ?????",	mulh.w,		3R,		int64_t product = ((int64_t)(int32_t)GR(rj)) * ((int64_t)(int32_t)GR(rk)); GR(rd) = ((product & 0xffffffff00000000) >> 32) & 0xffffffff);
	INSTPAT("00000000000111010 ????? ????? ?????",	mulh.wu,	3R,		uint64_t product = ((unsigned)GR(rj)) * ((unsigned)GR(rk)); GR(rd) = ((product & 0xffffffff00000000) >> 32) & 0xffffffff);
	INSTPAT("00000000001000000 ????? ????? ?????",	div.w,		3R,		int64_t quotient = ((int64_t)(int32_t)GR(rj)) / ((int64_t)(int32_t)GR(rk)); GR(rd) = quotient & 0xffffffff);
	INSTPAT("00000000001000001 ????? ????? ?????",	mod.w,		3R,		int64_t remainder = ((int64_t)(int32_t)GR(rj)) % ((int64_t)(int32_t)GR(rk)); GR(rd) = remainder & 0xffffffff);
	INSTPAT("00000000001000010 ????? ????? ?????",	div.wu,		3R,		uint64_t quotient = ((unsigned)GR(rj)) / ((unsigned)GR(rk)); GR(rd) = quotient & 0xffffffff);
	INSTPAT("00000000001000011 ????? ????? ?????",	mod.wu,		3R,		uint64_t remainder = ((unsigned)GR(rj)) % ((unsigned)GR(rk)); GR(rd) = remainder & 0xffffffff);
	
	/* type 1RSI20 */
	INSTPAT("0001010 ???????????????????? ?????",	lu12i.w,	1RSI20,		GR(rd) = imm);
	INSTPAT("0001110 ???????????????????? ?????",	pcaddu12i.w,	1RSI20,		GR(rd) = s->pc + imm);
	
	/* type 2RUI5 */
	INSTPAT("00000000010000 001 ????? ????? ?????",	slli.w,		2RUI5,		GR(rd) = GR(rj) << (imm & 0x1f));
	INSTPAT("00000000010001 001 ????? ????? ?????",	srli.w,		2RUI5,		GR(rd) = ((unsigned)GR(rj)) >> (imm & 0x1f));
	INSTPAT("00000000010010 001 ????? ????? ?????",	srai.w,		2RUI5,		GR(rd) = ((signed)GR(rj)) >> (imm & 0x1f));

	/* type 2RSI12 */
	INSTPAT("0000001000 ???????????? ????? ?????",	slti,		2RSI12,		GR(rd) = (signed)GR(rj) < (signed)imm ? 1 : 0);
	INSTPAT("0000001001 ???????????? ????? ?????",	sltui,		2RSI12,		GR(rd) = (unsigned)GR(rj) < (unsigned)imm ? 1 : 0);
	INSTPAT("0000001010 ???????????? ????? ?????",	addi.w,		2RSI12,		uint64_t tmp = ((int64_t)(int32_t)GR(rj)) + ((int64_t)(int32_t)imm); GR(rd) = tmp & 0xffffffff);
	INSTPAT("0010100000 ???????????? ????? ?????",	ld.b,		2RSI12,		uint8_t byte = Memory_Load(GR(rj) + imm, 1); GR(rd) = SEXT(byte, 8));
	INSTPAT("0010100001 ???????????? ????? ?????",	ld.h,		2RSI12,		uint16_t halfword = Memory_Load(GR(rj) + imm, 2); GR(rd) = SEXT(halfword, 16));
	INSTPAT("0010100010 ???????????? ????? ?????",	ld.w,		2RSI12,		GR(rd) = Memory_Load(GR(rj) + imm, 4));
	INSTPAT("0010101000 ???????????? ????? ?????",	ld.bu,		2RSI12,		uint8_t byte = Memory_Load(GR(rj) + imm, 1); GR(rd) = byte);
	INSTPAT("0010101001 ???????????? ????? ?????",	ld.hu,		2RSI12,		uint16_t halfword = Memory_Load(GR(rj) + imm, 2); GR(rd) = halfword);
	INSTPAT("0010100100 ???????????? ????? ?????",	st.b,		2RSI12,		Memory_Store(GR(rj) + imm, 1, GR(rd) & 0xff));
	INSTPAT("0010100101 ???????????? ????? ?????",	st.h,		2RSI12,		Memory_Store(GR(rj) + imm, 2, GR(rd) & 0xffff));
	INSTPAT("0010100110 ???????????? ????? ?????",	st.w,		2RSI12,		Memory_Store(GR(rj) + imm, 4, GR(rd)));
	
	/* type 2RUI12*/
	INSTPAT("0000001101 ???????????? ????? ?????",	andi.w,		2RUI12,		GR(rd) = GR(rj) & imm);
	INSTPAT("0000001110 ???????????? ????? ?????",	ori.w,		2RUI12,		GR(rd) = GR(rj) | imm);
	INSTPAT("0000001111 ???????????? ????? ?????",	xori.w,		2RUI12,		GR(rd) = GR(rj) ^ imm);

	/* type 2RSI16 */
	INSTPAT("010011 ???????????????? ????? ?????",	jirl,		2RI16,		GR(rd) = s->pc + 4; s->dnpc = GR(rj) + imm);
	INSTPAT("010110 ???????????????? ????? ?????",	beq,		2RI16,		s->dnpc = GR(rj) == GR(rd) ? s->pc + imm : s->snpc);
	INSTPAT("010111 ???????????????? ????? ?????",	bne,		2RI16,		s->dnpc = GR(rj) != GR(rd) ? s->pc + imm : s->snpc);
	INSTPAT("011000 ???????????????? ????? ?????",	blt,		2RI16,		s->dnpc = ((signed)GR(rj)) < ((signed)GR(rd)) ? s->pc + imm : s->snpc);
	INSTPAT("011001 ???????????????? ????? ?????",	bge,		2RI16,		s->dnpc = ((signed)GR(rj)) >= ((signed)GR(rd)) ? s->pc + imm : s->snpc);
	INSTPAT("011010 ???????????????? ????? ?????",	bltu,		2RI16,		s->dnpc = ((unsigned)GR(rj)) < ((unsigned)GR(rd)) ? s->pc + imm : s->snpc);
	INSTPAT("011011 ???????????????? ????? ?????",	bgeu,		2RI16,		s->dnpc = ((unsigned)GR(rj)) >= ((unsigned)GR(rd)) ? s->pc + imm : s->snpc);


	/* type I26 */
	INSTPAT("010100 ???????????????? ??????????",	b,		I26,		s->dnpc = s->pc + imm);
	INSTPAT("010101 ???????????????? ??????????",	bl,		I26,		GR(1) = s->pc + 4; s->dnpc = s->pc + imm);

	/* type None */
	INSTPAT("11111 00000000000000000 00000 ?????",	ntrap,		NTRAP,		NEMUTRAP(s->pc, GR(rd)));
	INSTPAT("????????????????????????????????",	inv,		N,		INV(s->pc));
	INSTPAT_END();

#ifdef CONFIG_ITRACE_COND
	if (ITRACE_COND)
		log_write("0x%x:\t%08x\t%s\trj: %d\trk: %d\trd: %d\timm: 0x%x\n", s->pc, s->isa.inst.val, instr, rj, rk, rd, imm);
#endif

	GR(0) = 0; // reset $zero to 0

	return 0;
}

int isa_exec_once(Decode *s) {
	s->isa.inst.val = inst_fetch(&s->snpc, 4);
	return decode_exec(s);
}
