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

#ifndef __CPU_IFETCH_H__

#include <memory/vaddr.h>

static inline uint32_t inst_fetch0(vaddr_t *pc, int len) {
  uint32_t inst = vaddr_ifetch(*pc, len);
  if (unlikely(nemu_state.state == NEMU_ABORT))
    panic("INSTRUNCTION FETCH FAILS! pc = " FMT_PADDR, *pc);
  (*pc) += len;
  return inst;
}

static inline uint32_t inst_fetch(vaddr_t *pc, int len) {
  if (((uintptr_t) *pc) == 0x830001dc) {
    panic("pc = " FMT_PADDR, *pc);
  }
  uint32_t inst = vaddr_ifetch(*pc, len);
  (*pc) += len;
  return inst;
}

#endif
