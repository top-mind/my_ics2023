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
#include <cpu/difftest.h>
#include "../local-include/reg.h"

bool isa_difftest_checkregs(CPU_state *ref_r, vaddr_t pc) {
  bool res = true;
  for (int i = 0; i < ARRLEN(((CPU_state *)0)->gpr); ++i) {
    if (ref_r->gpr[i] != cpu.gpr[i]) {
      printf("gpr[%d]: 0x%x, ref: 0x%x, pc: 0x%x\n", i, cpu.gpr[i], ref_r->gpr[i], pc);
      res = false;
    }
  }
  if (ref_r->mepc != cpu.mepc) {
    printf("mepc: 0x%x, ref: 0x%x, pc: 0x%x\n", cpu.mepc, ref_r->mepc, pc);
    res = false;
  }
  if (ref_r->mstatus != cpu.mstatus) {
    printf("mstatus: 0x%x, ref: 0x%x, pc: 0x%x\n", cpu.mstatus, ref_r->mstatus, pc);
    res = false;
  }
  if (ref_r->mtvec != cpu.mtvec) {
    printf("mtvec: 0x%x, ref: 0x%x, pc: 0x%x\n", cpu.mtvec, ref_r->mtvec, pc);
    res = false;
  }
  if (ref_r->mcause != cpu.mcause) {
    printf("mcause: 0x%x, ref: 0x%x, pc: 0x%x\n", cpu.mcause, ref_r->mcause, pc);
    res = false;
  }
  if (ref_r->pc != cpu.pc) {
    printf("pc: 0x%x, ref: 0x%x, pc: 0x%x\n", cpu.pc, ref_r->pc, pc);
    res = false;
  }
  if (ref_r->satp != cpu.satp) {
    printf("satp: 0x%x, ref: 0x%x, pc: 0x%x\n", cpu.satp, ref_r->satp, pc);
    res = false;
  }
  if (ref_r->privilege != cpu.privilege) {
    printf("privilege: 0x%x, ref: 0x%x, pc: 0x%x\n", cpu.privilege, ref_r->privilege, pc);
    res = false;
  }
  return res;
}

void isa_difftest_attach() {}
