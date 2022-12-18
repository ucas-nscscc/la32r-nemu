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

#include "isa-def.h"
#include "isa.h"
#include "local-include/reg.h"
#include "utils.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>
#include <stdint.h>
#include <stdio.h>

#define GR(name) gpr(name)
#define CSR(name) csr(name)
#define Memory_Load vaddr_read
#define Memory_Store vaddr_write

enum {
	TYPE_3RI0,
	TYPE_2RI0,
	TYPE_2RUI5,
	TYPE_2RSI12,
	TYPE_2RUI12,
	TYPE_2RI16,
	TYPE_1RSI20,
	TYPE_0RI26,
	TYPE_1CSR1R,
	TYPE_1CSR2R,
	TYPE_N, // none
};

#define get_gpr_idx(ptr, base)				\
({							\
	int __base = base;				\
	int __idx = BITS(i, (__base + 4), __base);	\
	*ptr = __idx;					\
})
#define get_csr_idx(ptr, base)				\
({							\
	int __base = base;				\
	int __idx = BITS(i, (__base + 14), __base);	\
	*ptr = __idx;					\
})
#define RJ() get_gpr_idx(rj, 5)
#define RK() get_gpr_idx(rk, 10)
#define RD() get_gpr_idx(rd, 0)
#define RC() get_csr_idx(csr, 10)
#define _3R() { RJ(); RK(); RD(); }
#define _2R() { RJ(); RD(); }
#define _1R() { RD(); }
#define _0R()
#define _1CSR() { RC(); }

#define I0()
#define SI20() do { *imm = SEXT(BITS(i, 24, 5), 20) << 12; } while(0)
#define SI12() do { *imm = SEXT(BITS(i, 21, 10), 12); } while(0)
#define UI12() do { *imm = BITS(i, 21, 10); } while(0)
#define UI5() do { *imm = BITS(i, 14, 10); } while(0)
#define I16() do { *imm = SEXT((BITS(i, 25, 10) << 2), 18); } while(0)
#define I26() do { *imm = SEXT((((BITS(i, 9, 0) << 16) | BITS(i, 25, 10)) << 2), 28); } while(0)

#define prepare_ops(gpr_type, imm_type)	\
	case TYPE_##gpr_type##imm_type:	\
		_##gpr_type();		\
		imm_type();		\
		break;
#define csr_prepare_ops(csr_type, gpr_type)	\
	case TYPE_##csr_type##gpr_type:		\
		_##gpr_type();			\
		_##csr_type();			\
		break;

#define NR_US (SC_FREQ / (1 MHz))

static void decode_operand(Decode *s, int type, int *rj, int *rk, int *rd, int *csr, word_t *imm) {
	uint32_t i = s->isa.inst.val;
	switch (type) {
		prepare_ops(3R,I0);
		prepare_ops(2R, I0);
		prepare_ops(2R,UI5);
		prepare_ops(2R,SI12);
		prepare_ops(2R,UI12);
		prepare_ops(2R,I16);
		prepare_ops(1R,SI20);
		prepare_ops(0R,I26);
		csr_prepare_ops(1CSR,1R);
		csr_prepare_ops(1CSR,2R);
	}
}

static int decode_exec(Decode *s) {
	int rj = 0;
	int rk = 0;
	int rd = 0;
	int csr = 0;
	word_t imm = 0;
	char instr[32];

	/* get outer interrupt sample */
	CSR(CSR_ESTAT) = (cpu.intr & 0x1ffc) | (CSR(CSR_ESTAT) & ~0x1ffc);
	if ((CSR(CSR_ESTAT) & CSR(CSR_ECFG) & 0x1fff) != 0 && (CSR(CSR_CRMD) & 0x4) != 0) {
		cpu.emask |= 0x1;
		cpu.esubmask |= 0x1;
		cpu.ex_taken = 1;
	}

	if (cpu.ex_taken) {
		goto finish_exec_instr;
	}

	s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->isa.inst.val)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ )			\
{										\
	const char inst_name[] = #name;						\
	strcpy(instr, inst_name);						\
	decode_operand(s, concat(TYPE_, type), &rj, &rk, &rd, &csr, &imm);	\
	__VA_ARGS__ ;								\
}

	INSTPAT_START();
	/*	Inst Pattern				Inst Name	Inst Type	Inst Op */
	/* type 3R */
	INSTPAT("00000000000100000 ????? ????? ?????",	add.w,		3RI0,		uint64_t tmp = ((int64_t)(int32_t)GR(rj)) + ((int64_t)(int32_t)GR(rk)); GR(rd) = tmp & 0xffffffff);
	INSTPAT("00000000000100010 ????? ????? ?????",	sub.w,		3RI0,		uint64_t tmp = ((int64_t)(int32_t)GR(rj)) - ((int64_t)(int32_t)GR(rk)); GR(rd) = tmp & 0xffffffff);
	INSTPAT("00000000000100100 ????? ????? ?????",	slt.w,		3RI0,		GR(rd) = (signed)GR(rj) < (signed)GR(rk) ? 1 : 0);
	INSTPAT("00000000000100101 ????? ????? ?????",	sltu.w,		3RI0,		GR(rd) = (unsigned)GR(rj) < (unsigned)GR(rk) ? 1 : 0);
	INSTPAT("00000000000101000 ????? ????? ?????",	nor.w,		3RI0,		GR(rd) = ~(GR(rj) | GR(rk)));
	INSTPAT("00000000000101001 ????? ????? ?????",	and.w,		3RI0,		GR(rd) = GR(rj) & GR(rk));
	INSTPAT("00000000000101010 ????? ????? ?????",	or.w,		3RI0,		GR(rd) = GR(rj) | GR(rk));
	INSTPAT("00000000000101011 ????? ????? ?????",	xor.w,		3RI0,		GR(rd) = GR(rj) ^ GR(rk));
	INSTPAT("00000000000101110 ????? ????? ?????",	sll.w,		3RI0,		GR(rd) = GR(rj) << (GR(rk) & 0x1f));
	INSTPAT("00000000000101111 ????? ????? ?????",	srl.w,		3RI0,		GR(rd) = ((unsigned)GR(rj)) >> (GR(rk) & 0x1f));
	INSTPAT("00000000000110000 ????? ????? ?????",	sra.w,		3RI0,		GR(rd) = ((signed)GR(rj)) >> (GR(rk) & 0x1f));
	INSTPAT("00000000000111000 ????? ????? ?????",	mul.w,		3RI0,		int64_t product = ((int64_t)(int32_t)GR(rj)) * ((int64_t)(int32_t)GR(rk)); GR(rd) = product & 0xffffffff);
	INSTPAT("00000000000111001 ????? ????? ?????",	mulh.w,		3RI0,		int64_t product = ((int64_t)(int32_t)GR(rj)) * ((int64_t)(int32_t)GR(rk)); GR(rd) = ((product & 0xffffffff00000000) >> 32) & 0xffffffff);
	INSTPAT("00000000000111010 ????? ????? ?????",	mulh.wu,	3RI0,		uint64_t product = ((unsigned)GR(rj)) * ((unsigned)GR(rk)); GR(rd) = ((product & 0xffffffff00000000) >> 32) & 0xffffffff);
	INSTPAT("00000000001000000 ????? ????? ?????",	div.w,		3RI0,		int64_t quotient = ((int64_t)(int32_t)GR(rj)) / ((int64_t)(int32_t)GR(rk)); GR(rd) = quotient & 0xffffffff);
	INSTPAT("00000000001000001 ????? ????? ?????",	mod.w,		3RI0,		int64_t remainder = ((int64_t)(int32_t)GR(rj)) % ((int64_t)(int32_t)GR(rk)); GR(rd) = remainder & 0xffffffff);
	INSTPAT("00000000001000010 ????? ????? ?????",	div.wu,		3RI0,		uint64_t quotient = ((unsigned)GR(rj)) / ((unsigned)GR(rk)); GR(rd) = quotient & 0xffffffff);
	INSTPAT("00000000001000011 ????? ????? ?????",	mod.wu,		3RI0,		uint64_t remainder = ((unsigned)GR(rj)) % ((unsigned)GR(rk)); GR(rd) = remainder & 0xffffffff);
	
	/* type 1RSI20 */
	INSTPAT("0001010 ???????????????????? ?????",	lu12i.w,	1RSI20,		GR(rd) = imm);
	INSTPAT("0001110 ???????????????????? ?????",	pcaddu12i.w,	1RSI20,		GR(rd) = s->pc + imm);
	
	/* type 2RI0 */
	INSTPAT("0000000000000000011000 ????? ?????",	rdcntvl.w,	2RI0,		GR(rd) = cpu.stable_counter & 0xffffffff);
	INSTPAT("0000000000000000011001 ????? ?????",	rdcntvh.w,	2RI0,		GR(rd) = (cpu.stable_counter >> 32) & 0xffffffff);

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
	INSTPAT("010100 ???????????????? ??????????",	b,		0RI26,		s->dnpc = s->pc + imm);
	INSTPAT("010101 ???????????????? ??????????",	bl,		0RI26,		GR(1) = s->pc + 4; s->dnpc = s->pc + imm);

	/* type 1CSR1R */
	INSTPAT("00000100 ?????????????? 00000 ?????",	csrrd,		1CSR1R,		GR(rd) = CSR(csr));
	INSTPAT("00000100 ?????????????? 00001 ?????",	csrwr,		1CSR1R,		word_t tmp = GR(rd); GR(rd) = CSR(csr); CSR(csr) = tmp);
	
	/* type 1CSR2R */
	INSTPAT("00000100 ?????????????? ????? ?????",	csrxchg,	1CSR2R,		word_t tmp = GR(rd); GR(rd) = CSR(csr); CSR(csr) = (tmp & GR(rj)) | (CSR(csr) & ~GR(rj)));
	
	INSTPAT("0000011001001000001110 00000 00000",	ertn,		N,		csr(CSR_CRMD) = (csr(CSR_PRMD) & 0x7) | (csr(CSR_CRMD) & ~(0x7)); s->dnpc = CSR(CSR_ERA));

	/* type None */
	INSTPAT("11111 00000000000000000 00000 ?????",	ntrap,		1RSI20,		NEMUTRAP(s->pc, GR(rd)));
	INSTPAT("????????????????????????????????",	inv,		N,		INV(s->pc));
	INSTPAT_END();

#ifdef CONFIG_ITRACE_COND
	if (ITRACE_COND) {
		char tmp[128];
		sprintf(tmp, "0x%x:\t%08x\t%s\trj: %d\trk: %d\trd: %d\timm: 0x%x", s->pc, s->isa.inst.val, instr, rj, rk, rd, imm);
		log_write("%s\n", tmp);
		strcpy(iring[iring_ptr], tmp);
		iring_ptr = (iring_ptr + 1) % IRING_SIZE;
	}
#endif

finish_exec_instr:
	/* reset gpr r0 to zero */
	GR(0) = 0;
	
	/* update cpu exception status */
	if (cpu.ex_taken) {
		s->dnpc = isa_raise_intr(0, cpu.pc);
	}

	/* update stable counter */
	cpu.stable_counter = get_time() * NR_US;

	return 0;
}

int isa_exec_once(Decode *s) {
	cpu.emask = 0;
	cpu.esubmask = 0;
	cpu.ex_taken = 0;
	if (s->snpc % 4 != 0) {
		cpu.emask |= (0x1 << ECODE_ADEF);
		cpu.esubmask |= 0x1;
		cpu.ex_taken = 1;
	} else {
		s->isa.inst.val = inst_fetch(&s->snpc, 4);
	}
	return decode_exec(s);
}
