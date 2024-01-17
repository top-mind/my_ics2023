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

#include <cpu/cpu.h>
#include <isa.h>
#include <memory/vaddr.h>
#include <memory/paddr.h>

int isa_mmu_check(vaddr_t vaddr, int len, int type) {
  return cpu.mode ? MMU_TRANSLATE : MMU_DIRECT;
}

typedef union {
  word_t val;
  struct {
    uint32_t v : 1;
    uint32_t r : 1;
    uint32_t w : 1;
    uint32_t x : 1;
    uint32_t u : 1;
    uint32_t g : 1;
    uint32_t a : 1;
    uint32_t d : 1;
    uint32_t rsw : 2;
    uint32_t ppn0 : 10;
    uint32_t ppn1 : 12;
  };
} pte_t;

#define LEVELS 2
#define PTE_SIZE 4
#define vpn(vaddr, i) ((vaddr >> (12 + 10 * i)) & 0x3ff)
/* See isa.h
 * follow riscv-privileged-v20211203.pdf 4.3.2
 */
paddr_t isa_mmu_translate(vaddr_t vaddr, int len, int type) {
  word_t a = cpu.ppn * PAGE_SIZE;
  int i = LEVELS - 1;
  int valid = 0;
  pte_t pte;
step2:
  pte.val = paddr_read(a + vpn(vaddr, i) * PTE_SIZE, PTE_SIZE);
  if (pte.v == 0 || (pte.x == 0 && pte.w == 1)) {
    return MEM_RET_FAIL;
  }
  if (pte.r == 1 || pte.x == 1) {
    goto step5;
  } else {
    i = i - 1;
    if (i < 0) {
      return MEM_RET_FAIL;
    }
    a = BITS(pte.val, 31, 12) * PAGE_SIZE;
    goto step2;
  }
step5:
  switch (type) {
    case MEM_TYPE_READ: valid = pte.r; break;
    case MEM_TYPE_WRITE: valid = pte.w; break;
    case MEM_TYPE_IFETCH: valid = pte.x; break;
  }
  if (valid == 0) {
    return MEM_RET_FAIL;
  }
  if (i > 0 && pte.ppn0 != 0) {
    return MEM_RET_FAIL;
  }
  // step7 pte.a pte.d (scheme 1)
  if (pte.a == 0 || (type == MEM_TYPE_WRITE && pte.d == 0)) {
    return MEM_RET_FAIL;
  }
  paddr_t pgaddr = BITS(pte.val, 31, 12) * PAGE_SIZE;
  if (i > 0) {
    pgaddr += BITS(vaddr, 21, 12) * PAGE_SIZE;
  }
  return pgaddr| MEM_RET_OK;
}
