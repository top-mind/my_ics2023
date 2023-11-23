#include "cpu/ifetch.h"
#include "memory/host.h"
#include "memory/paddr.h"
#include "sdb.h"
#define NR_BP 32
static int end;
unsigned cnt_bp;
BP bp_pool[NR_BP];

static inline void new_breakpoint(char *s) {
  BP *p = &bp_pool[end];
  paddr_t addr = 0; // TODO
  p->b.addr = addr;
  p->b.raw_instr = inst_fetch(&addr, sizeof p->b.raw_instr);
  // clang-format off
  uint32_t data = MUXDEF(CONFIG_ISA_x86, 0xcc,          // int3
                  MUXDEF(CONFIG_ISA_mips32, ,           // sdbbp TODO
                  MUXDEF(CONFIG_ISA_riscv, 0x00100073,  // ebreak
                  MUXDEF(CONFIG_ISA_loongarch32r, ,     // LOONGARCH32 break 0
                  ))));
  // clang-format on
  host_write(guest_to_host(addr), sizeof p->b.raw_instr, data);
}

static inline void new_watchpoint(char *s) {
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
}

int new_bp(char *s, bool is_watchpoint) {
  if (end == -1) return -1;
  bp_pool[end].num = ++cnt_bp;
  bp_pool[end].is_watchpoint = is_watchpoint;
  bp_pool[end].hint = s;
  bp_pool[end].hit = 0;
  if (is_watchpoint)
    new_watchpoint(s);
  else
    new_breakpoint(s);
  end = bp_pool[end].next;
  return cnt_bp;
}

int breakat(void *dummy) {
  if (end == -1) return -1;
  return 0;
}
