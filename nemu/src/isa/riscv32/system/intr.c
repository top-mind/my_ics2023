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

#include <isa.h>

#define IRQ_TIMER 0x80000007  // for riscv32

word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  cpu.mepc = epc;
  cpu.mcause = NO;
  cpu.mpie = cpu.mie;
  cpu.mie = 0;
  cpu.mpp = cpu.prv;
  cpu.prv = 3;

  if (cpu.mtvec & 0x3) {
    word_t base = BITS(cpu.mtvec, 31, 2);
    word_t interrupt = BITS(NO, 31, 31);
    word_t ecode = BITS(NO, 30, 0);
    if (interrupt)
      return base + 4 * ecode;
    else
      return base;
  }
  return cpu.mtvec;
}

word_t isa_query_intr() {
  if (cpu.mie)
    return IRQ_TIMER;
  else
    return INTR_EMPTY;
  if (cpu.INTR && cpu.mie) {
    cpu.INTR = 0;
    return IRQ_TIMER;
  }
  return INTR_EMPTY;
}
