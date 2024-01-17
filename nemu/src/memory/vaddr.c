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
#include <memory/vaddr.h>
#include <memory/paddr.h>
#include <cpu/cpu.h>

static word_t vaddr_read_r(vaddr_t addr, int len, int type) {
  if (isa_mmu_check(addr, len, type) == MMU_TRANSLATE) {
    paddr_t res = isa_mmu_translate(addr, len, type);
    if ((res & PAGE_MASK) == MEM_RET_OK) {
      paddr_t pa = (res & ~PAGE_MASK) | (addr & PAGE_MASK);
      return paddr_read(pa, len);
    }
    panic("vaddr_read: error vaddr = " FMT_WORD ", len=%d, type = %d, mmu retrun " FMT_WORD "\n",
          addr, len, type, res);
  }
  return paddr_read(addr, len);
}

word_t vaddr_ifetch(vaddr_t addr, int len) { return vaddr_read_r(addr, len, MEM_TYPE_IFETCH); }

word_t vaddr_read(vaddr_t addr, int len) { return vaddr_read_r(addr, len, MEM_TYPE_READ); }

void vaddr_write(vaddr_t addr, int len, word_t data) {
  if (isa_mmu_check(addr, len, MEM_TYPE_WRITE) == MMU_TRANSLATE) {
    paddr_t res = isa_mmu_translate(addr, len, MEM_TYPE_WRITE);
    if ((res & PAGE_MASK) == MEM_RET_OK) {
      paddr_t pa = (res & ~PAGE_MASK) | (addr & PAGE_MASK);
      paddr_write(pa, len, data);
    } else {
      panic("vaddr_write: error vaddr = " FMT_WORD ", len=%d, data=" FMT_WORD
            ", mmu retrun " FMT_WORD "\n",
            addr, len, data, res);
      set_nemu_state(NEMU_ABORT, cpu.pc, ABORT_MEMIO);
    }
  } else {
    paddr_write(addr, len, data);
  }
}
