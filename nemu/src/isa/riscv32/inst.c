/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
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

#include "isa.h"
#include "local-include/reg.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>
#include <stdint.h>

#define PRIV_LOW 3

#define R(i) gpr(i)
#define Mr   vaddr_read
#define Mw   vaddr_write

enum {
  TYPE_R,
  TYPE_I,
  TYPE_IS, // variant i type
  TYPE_CSRw,
  TYPE_CSRwi,
  TYPE_S,
  TYPE_B,
  TYPE_U,
  TYPE_J,
  TYPE_N, // none
};

#define src1R() \
  do { *src1 = R(rs1); } while (0)
#define src2R() \
  do { *src2 = R(rs2); } while (0)
#define immI() \
  do { *imm = SEXT(BITS(i, 31, 20), 12); } while (0)
#define immIS() \
  do { *imm = BITS(i, 24, 20); } while (0)
#define immU() \
  do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while (0)
#define immS() \
  do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while (0)
#define immB()                                                                           \
  do {                                                                                   \
    *imm = SEXT(BITS(i, 31, 31), 1) << 12 | BITS(i, 30, 25) << 5 | BITS(i, 7, 7) << 11 | \
           BITS(i, 11, 8) << 1;                                                          \
  } while (0)
#define immJ()                                                                             \
  do {                                                                                     \
    *imm = SEXT(BITS(i, 31, 31), 1) << 20 | BITS(i, 30, 21) << 1 | BITS(i, 20, 20) << 11 | \
           BITS(i, 19, 12) << 12;                                                          \
  } while (0)
#define csr()                                 \
  do {                                        \
    switch (BITS(i, 31, 20)) {                \
      case 0x300: *csr = &cpu.mstatus; break; \
      case 0x305: *csr = &cpu.mtvec; break;   \
      case 0x341: *csr = &cpu.mepc; break;    \
      case 0x342: *csr = &cpu.mcause; break;  \
      default: INV(s->pc);                    \
    }                                         \
    R(*rd) = **csr;                           \
  } while (0)

static void decode_operand(Decode *s, int *rd, word_t *src1, word_t *src2, word_t *imm, word_t **csr,
                           int type) {
  uint32_t i = s->isa.inst.val;
  int rs1 = BITS(i, 19, 15);
  int rs2 = BITS(i, 24, 20);
  *rd = BITS(i, 11, 7);
  switch (type) {
    // RIISSBUJ CSRw
    case TYPE_R:
      src1R();
      src2R();
      break;
    case TYPE_I:
      src1R();
      immI();
      break;
    case TYPE_CSRw:
      src1R();
      csr();
      break;
    case TYPE_CSRwi:
      *imm = rs1;
      csr();
      break;
    case TYPE_IS:
      src1R();
      immIS();
      break;
    case TYPE_S:
      src1R();
      src2R();
      immS();
      break;
    case TYPE_B:
      src1R();
      src2R();
      immB();
      break;
    case TYPE_U: immU(); break;
    case TYPE_J: immJ(); break;
  }
    }

static int decode_exec(Decode *s) {
  int rd = 0;
  word_t src1 = 0, src2 = 0, imm = 0;
  word_t *csr;
  s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->isa.inst.val)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */)         \
  {                                                                  \
    decode_operand(s, &rd, &src1, &src2, &imm, &csr, concat(TYPE_, type)); \
    __VA_ARGS__;                                                     \
  }
#define INSTBRANCH                                            \
  ({                                                          \
    int __r = 0;                                              \
    switch (BITS(INSTPAT_INST(s), 14, 12)) {                  \
      case 0b000: __r = src1 == src2; break;                  \
      case 0b001: __r = src1 != src2; break;                  \
      case 0b100: __r = (sword_t)src1 < (sword_t)src2; break; \
      case 0b101: __r = (word_t)src1 >= (sword_t)src2; break; \
      case 0b110: __r = src1 < src2; break;                   \
      case 0b111: __r = src1 >= src2; break;                  \
      default: INV(s->pc);                                    \
    }                                                         \
    __r;                                                      \
  })

  INSTPAT_START();
  INSTPAT("??????? ????? ????? ??? ????? 01101 11", lui, U, R(rd) = imm);
  INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc, U, R(rd) = s->pc + imm);
  INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal, J, R(rd) = s->snpc, s->dnpc = s->pc + imm);
  INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr, I, R(rd) = s->snpc,
          s->dnpc = (src1 + imm) >> 1 << 1);
  // INSTPAT("??????? ????? ????? ??? ????? 11000 11", branch, B, if (INSTBRANCH) s->dnpc = s->pc + imm);
  INSTPAT("??????? ????? ????? 000 ????? 11000 11", beq, B,
          if (src1 == src2) s->dnpc = s->pc + imm);
  INSTPAT("??????? ????? ????? 001 ????? 11000 11", bne, B,
          if (src1 != src2) s->dnpc = s->pc + imm);
  INSTPAT("??????? ????? ????? 100 ????? 11000 11", blt, B,
          if ((sword_t)src1 < (sword_t)src2) s->dnpc = s->pc + imm);
  INSTPAT("??????? ????? ????? 101 ????? 11000 11", bge, B,
          if ((sword_t)src1 >= (sword_t)src2) s->dnpc = s->pc + imm);
  INSTPAT("??????? ????? ????? 110 ????? 11000 11", bltu, B,
          if (src1 < src2) s->dnpc = s->pc + imm);
  INSTPAT("??????? ????? ????? 111 ????? 11000 11", bgeu, B,
          if (src1 >= src2) s->dnpc = s->pc + imm);

  INSTPAT("??????? ????? ????? 000 ????? 00000 11", lb, I, R(rd) = SEXT(Mr(src1 + imm, 1), 8));
  INSTPAT("??????? ????? ????? 001 ????? 00000 11", lh, I, R(rd) = SEXT(Mr(src1 + imm, 2), 16));
  INSTPAT("??????? ????? ????? 010 ????? 00000 11", lw, I, R(rd) = Mr(src1 + imm, 4));
  INSTPAT("??????? ????? ????? 100 ????? 00000 11", lbu, I, R(rd) = Mr(src1 + imm, 1));
  INSTPAT("??????? ????? ????? 101 ????? 00000 11", lhu, I, R(rd) = Mr(src1 + imm, 2));
  INSTPAT("??????? ????? ????? 000 ????? 01000 11", sb, S, Mw(src1 + imm, 1, src2));
  INSTPAT("??????? ????? ????? 001 ????? 01000 11", sh, S, Mw(src1 + imm, 2, src2));
  INSTPAT("??????? ????? ????? 010 ????? 01000 11", sw, S, Mw(src1 + imm, 4, src2));
  INSTPAT("??????? ????? ????? 000 ????? 00100 11", addi, I, R(rd) = src1 + imm);
  INSTPAT("??????? ????? ????? 010 ????? 00100 11", slti, I, R(rd) = (sword_t)src1 < (sword_t)imm);
  INSTPAT("??????? ????? ????? 011 ????? 00100 11", sltiu, I, R(rd) = src1 < imm);
  INSTPAT("??????? ????? ????? 100 ????? 00100 11", xori, I, R(rd) = src1 ^ imm);
  INSTPAT("??????? ????? ????? 110 ????? 00100 11", ori, I, R(rd) = src1 | imm);
  INSTPAT("??????? ????? ????? 111 ????? 00100 11", andi, I, R(rd) = src1 & imm);
  INSTPAT("0000000 ????? ????? 001 ????? 00100 11", slli, IS, R(rd) = src1 << imm);
  INSTPAT("0000000 ????? ????? 101 ????? 00100 11", srli, IS, R(rd) = src1 >> imm);
  INSTPAT("0100000 ????? ????? 101 ????? 00100 11", srai, IS, R(rd) = (sword_t)src1 >> imm);
  INSTPAT("0000000 ????? ????? 000 ????? 01100 11", add, R, R(rd) = src1 + src2);
  INSTPAT("0100000 ????? ????? 000 ????? 01100 11", sub, R, R(rd) = src1 - src2);
  INSTPAT("0000000 ????? ????? 001 ????? 01100 11", sll, R, R(rd) = src1 << BITS(src2, 4, 0));
  INSTPAT("0000000 ????? ????? 010 ????? 01100 11", slt, R, R(rd) = (sword_t)src1 < (sword_t)src2);
  INSTPAT("0000000 ????? ????? 011 ????? 01100 11", sltu, R, R(rd) = src1 < src2);
  INSTPAT("0000000 ????? ????? 100 ????? 01100 11", xor, R, R(rd) = src1 ^ src2);
  INSTPAT("0000000 ????? ????? 101 ????? 01100 11", srl, R, R(rd) = src1 >> BITS(src2, 4, 0));
  INSTPAT("0100000 ????? ????? 101 ????? 01100 11", sra, R,
          R(rd) = (sword_t)src1 >> BITS(src2, 4, 0));
  INSTPAT("0000000 ????? ????? 110 ????? 01100 11", or, R, R(rd) = src1 | src2);
  INSTPAT("0000000 ????? ????? 111 ????? 01100 11", and, R, R(rd) = src1 & src2);
  INSTPAT("0000001 ????? ????? 000 ????? 01100 11 ", mul, R, R(rd) = src1 * src2);
  INSTPAT("0000001 ????? ????? 001 ????? 01100 11 ", mulh, R,
          R(rd) = SEXT(src1, 32) * SEXT(src2, 32) >> 32);
  INSTPAT("0000001 ????? ????? 010 ????? 01100 11 ", mulhsu, R,
          R(rd) = SEXT(src1, 32) * ZEXT(src2, 32) >> 32);
  INSTPAT("0000001 ????? ????? 011 ????? 01100 11 ", mulhu, R,
          R(rd) = ZEXT(src1, 32) * ZEXT(src2, 32) >> 32);
  INSTPAT("0000001 ????? ????? 100 ????? 01100 11 ", div, R,
          R(rd) = src2 == 0                         ? -1
                  : src1 == INT32_MIN && src2 == -1 ? INT32_MIN
                                                    : (sword_t)src1 / (sword_t)src2);
  INSTPAT("0000001 ????? ????? 101 ????? 01100 11 ", divu, R, R(rd) = src2 == 0 ? -1 : src1 / src2);
  INSTPAT("0000001 ????? ????? 110 ????? 01100 11 ", rem, R,
          R(rd) = src2 == 0                         ? src1
                  : src1 == INT32_MIN && src2 == -1 ? 0
                                                    : (sword_t)src1 % (sword_t)src2);
  INSTPAT("0000001 ????? ????? 111 ????? 01100 11 ", remu, R,
          R(rd) = src2 == 0 ? src1 : src1 % src2);
  INSTPAT("0000000 00000 00000 000 00000 11100 11", ecall, N, s->dnpc = isa_raise_intr(11, s->pc),
      cpu.mpie = cpu.mie, cpu.mie = 0, cpu.mpp = 3);
  // may call isa_raise_intr(3, s->pc) or NEMUINT based on sdb mode
  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak, N, NEMUINT(s->pc, R(10))); // x10 = a0

  INSTPAT("??????? ????? ????? 001 ????? 11100 11", csrrw, CSRw, *csr = src1);
  INSTPAT("??????? ????? ????? 010 ????? 11100 11", csrrs, CSRw, *csr |= src1);
  INSTPAT("??????? ????? ????? 011 ????? 11100 11", csrrc, CSRw, *csr &= ~src1);
  INSTPAT("??????? ????? ????? 101 ????? 11100 11", csrrwi, CSRwi, *csr = imm);
  INSTPAT("??????? ????? ????? 110 ????? 11100 11", csrrsi, CSRwi, *csr |= imm);
  INSTPAT("??????? ????? ????? 111 ????? 11100 11", csrrci, CSRwi, *csr &= ~imm);
  // The exact behaviour is MPP = PREV_LOW, MIE = MPIE, MPIE = 1
  INSTPAT("0011000 00010 00000 000 00000 11100 11", mret, N, s->dnpc = cpu.mepc,
          cpu.mpp = PRIV_LOW, cpu.mie = cpu.mpie, cpu.mpie = 1);
  INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv, N, INV(s->pc));
  INSTPAT_END();

  R(0) = 0; // reset $zero to 0

  return 0;
}

int isa_exec_once(Decode *s) {
  s->isa.inst.val = inst_fetch(&s->snpc, 4);
  return decode_exec(s);
}

#ifdef CONFIG_FTRACE

void ftrace_pop(vaddr_t pc, vaddr_t dnpc);
void ftrace_push(vaddr_t pc, vaddr_t dnpc);
void isa_ras_update(Decode *s) {
#define PUSH ftrace_push(s->pc, s->dnpc)
#define POP  ftrace_pop(s->pc, s->dnpc)
#undef INSTPAT_MATCH
#define INSTPAT_MATCH(s, ...) __VA_ARGS__
  INSTPAT_START(ras);
  INSTPAT("??????? ????? ????? ??? 00?01 11011 11", PUSH); // rd=x1/x5
  INSTPAT("??????? ????? 00101 000 00001 11001 11", POP, PUSH);
  INSTPAT("??????? ????? 00001 000 00101 11001 11", POP, PUSH);
  INSTPAT("??????? ????? ????? 000 00?01 11001 11", PUSH);
  INSTPAT("??????? ????? 00?01 000 ????? 11001 11", POP);
  INSTPAT_END(ras);
}
#endif
