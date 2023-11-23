#include "cpu/ifetch.h"
#include "memory/host.h"
#include "memory/paddr.h"
#include "sdb.h"
#define NR_BP 32
static int end;
unsigned cnt_bp;
BP bp_pool[NR_BP];

static inline bool new_breakpoint(char *s) {
  BP *p = &bp_pool[end];
  paddr_t addr = 0x80000000; // TODO
  if (!in_pmem(addr)) {
    printf("I found %s at " FMT_PADDR ", but address out of range\n"
           "May symbol table unmatch?\n",
           s, addr);
    return false;
  }
  p->b.addr = addr;
  p->b.raw_instr = inst_fetch(&addr, sizeof p->b.raw_instr);
  // clang-format off
  uint32_t data = MUXDEF(CONFIG_ISA_x86, 0xcc,          // int3
                  MUXDEF(CONFIG_ISA_mips32, ,           // sdbbp
                  MUXDEF(CONFIG_ISA_riscv, 0x00100073,  // ebreak
                  MUXDEF(CONFIG_ISA_loongarch32r, ,     // LOONGARCH32 break 0
                  ))));
  // clang-format on
  printf("%zd %x\n", sizeof p->b.raw_instr, data);
  host_write(guest_to_host(addr), sizeof p->b.raw_instr, data);
  printf("%x\n", inst_fetch(&addr, 4));
  return true;
}

static inline bool new_watchpoint(char *s) {
  BP *p = &bp_pool[end];
  // p->w->rpn; TODO
  // p->w->nr_rpn;
  // p->w->old_value;
  p->w.next = -1;
  int cur = end;
  while (-1 != (cur = bp_pool[cur].prev) && !bp_pool[cur].is_watchpoint)
    ;
  if (cur != -1) bp_pool[cur].next = end;
  p->w.prev = cur;
  return true;
}

int new_bp(char *s, bool is_watchpoint) {
  if (end == -1) return -1;
  bool suc = is_watchpoint ? new_watchpoint(s) : new_breakpoint(s);
  if (!suc)
    return -1;
  bp_pool[end].num = ++cnt_bp;
  bp_pool[end].is_watchpoint = is_watchpoint;
  bp_pool[end].hint = s;
  bp_pool[end].hit = 0;
  end = bp_pool[end].next;
  return cnt_bp;
}
