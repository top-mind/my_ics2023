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

#include <ctype.h>
#include <isa.h>
#include "local-include/reg.h"
#include <common.h>

const char *regs[] = {"$0", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
                      "a1", "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
                      "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
const char *pc_csrs[] = {"pc", "mepc", "mstatus", "mcause", "mtvec"};
#define PC_CSR(i) ((&cpu.pc)[i])
#define NR_PC_CSR ARRLEN(pc_csrs)

#define NR_REG MUXDEF(CONFIG_RVE, 16, 32)
void isa_reg_display() {
  for (int i = 0; i < NR_REG; i++) {
    // TODO elf symbol: covert pointer to function name
    // gdb info r:
    // rax            0x555555556569      93824992241001
    // rbx            0x0                 0
    // rcx            0x55555555cb78      93824992267128
    // rdx            0x7fffffffe130      140737488347440
    // rsi            0x7fffffffe118      140737488347416
    // rdi            0x2                 2
    // rbp            0x2                 0x2
    // rsp            0x7fffffffe008      0x7fffffffe008
    // r8             0x7ffff7e1af10      140737352150800
    // r9             0x7ffff7fc9040      140737353912384
    // r10            0x7ffff7fc3908      140737353890056
    // r11            0x7ffff7fde660      140737353999968
    // r12            0x7fffffffe118      140737488347416
    // r13            0x555555556569      93824992241001
    // r14            0x55555555cb78      93824992267128
    // r15            0x7ffff7ffd040      140737354125376
    // rip            0x555555556569      0x555555556569 <main>
    // eflags         0x246               [ PF ZF IF ]
    // cs             0x33                51
    // ss             0x2b                43
    // ds             0x0                 0
    // es             0x0                 0
    // fs             0x0                 0
    // gs             0x0                 0
    //
    // ra sp gp tp - hex - hex <func>
    // t* s* a*    - hex - decimal
    // ps/mstatus  - hex - [flags]
    printf("%-15s0x%-8" PRIx32 "\t%" PRIu32 "\n", regs[i], cpu.gpr[i], cpu.gpr[i]);
  }
  for (int i = 0; i < ARRLEN(pc_csrs); i++) {
    printf("%-15s0x%-8" PRIx32 "\t%" PRIu32 "\n", pc_csrs[i], PC_CSR(i), PC_CSR(i));
  }
}

// rv32 reg ABI name or sdb preserved name to value
// ***deprecated***
// use isa_reg_str2ptr
word_t isa_reg_str2val(const char *s, bool *success) {
  panic("deprecated");
  // 这是为了 watchpoint 速度考虑的写法。
  // 现在已经弃用
  // 应该用 isa_reg_str2ptr
  if (unlikely(s == NULL)) {
    *success = false;
    return 0;
  }
  for (int i = 1; i < NR_REG; i++) {
    if (strcmp(s, regs[i]) == 0) {
      *success = true;
      return cpu.gpr[i];
    }
  }
  // 处理cpu中其他寄存器如 mstatus
  //
  // NOTE: 和 gdb 的一致性: https://sourceware.org/gdb/current/onlinedocs/gdb.html/Registers.html
  //   GNU gdb: 除非有ABI名称冲突, 总是提供 4 个寄存器: pc sp fp ps
  if (likely((strcmp(s, "pc") == 0))) {
    *success = true;
    return cpu.pc;
  } else if (strcmp(s, "ps") == 0 || strcmp(s, "mstatus") == 0) {
    // nemu 里 ps 是 mstatus 的别名.
    *success = true;
    // mstatus not implemented, return 0
    return 0;
  } else if (strcmp(s, "fp") == 0) {
    // rv abi规范指出 THE PRESENCE OF A FRAME POINTER IS OPTIONAL. iF A FRAME POINTER EXISTS, IT
    // MUST RESIDE IN X8 (S0); THE REGISTER REMAINS CALLEE-SAVED.
    *success = true;
    return cpu.gpr[8];
  }

  *success = false;
  return 0;
}

word_t *isa_reg_str2ptr(const char *s) {
  for (int i = 0; i < NR_REG; i++) {
    if (strcmp(s, regs[i]) == 0) { return &cpu.gpr[i]; }
  }
  if (strcmp(s, "pc") == 0) {
    return &cpu.pc;
  } else if (strcmp(s, "ps") == 0 || strcmp(s, "mstatus") == 0) {
    return &cpu.mstatus;
  } else if (strcmp(s, "fp") == 0) {
    return &cpu.gpr[8];
  }
  return NULL;
}

bool isa_reg_load(FILE *fp) {
  int i, ch;
  for (i = 0; i < NR_REG + NR_PC_CSR; i++) {
    do {
      ch = fgetc(fp);
      if (ch == EOF) return false;
    } while (!isspace(ch));
    word_t *ptr = (word_t *) &cpu;
    if (fscanf(fp, "%x", &ptr[i]) != 1) return false;
    do ch = fgetc(fp);
    while (ch != '\n' && ch != EOF);
  }
  puts("GOOD");
  return i == NR_REG + NR_PC_CSR;
}
// vim: fenc=utf-8
