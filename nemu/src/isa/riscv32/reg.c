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

void isa_reg_display() {
  for (int i = 0; i < MUXDEF(CONFIG_RVE, 16, 32); i++) {
    printf("%-15s0x%-8x\t0x%x\n", regs[i], cpu.gpr[i], cpu.gpr[i]);
  }
}

word_t isa_reg_str2val(const char *s, bool *success) {
  if (unlikely(s == NULL || s[0] == '\0' || (s[0] == '0' && s[1] != '\0'))) {
    *success = false;
    return 0;
  }
  if (isdigit(s[0])) {
    int idx = s[0] & 15;
    if (s[1] == '\0') {
      *success = true;
      return cpu.gpr[idx];
    }
    if (unlikely(!isdigit(s[1]))) {
      *success = false;
      return 0;
    }
    idx = idx * 10 + (s[1] & 15);
    if (unlikely(idx >= 32 || s[2] != '\0')) {
      *success = false;
      return 0;
    }
    *success = true;
    return cpu.gpr[idx];
  }
  for (int i = 1; i < 32; i++) {
    if (strcmp(s, regs[i]) == 0) {
      *success = true;
      return cpu.gpr[i];
    }
  }
  if (likely((strcmp(s, "pc") == 0))) {
    *success = true;
    return cpu.pc;
  }
  *success = false;
  return 0;
}
