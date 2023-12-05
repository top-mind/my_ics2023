#include <elf-def.h>
#include <memory/host.h>
#include <memory/paddr.h>
#include "sdb.h"
#include <limits.h>

typedef struct breakpoint_node {
  struct breakpoint_node *prev, *next;
  unsigned NO;
  bool duplicate;
  paddr_t addr;
  MUXDEF(CONFIG_ISA_x86, uint8_t, uint32_t) raw_instr;
} bp_t;

typedef struct watchpoint_node {
  struct watchpoint_node *prev, *next;
  unsigned NO;
  char *expr;
  rpn_t *rpn;
  size_t nr_rpn;
  eval_t old_value;
} wp_t;

static bp_t *bp_nil;
static wp_t *wp_nil;

size_t nr_breakpoints;

#define LEN_BREAK_INST (sizeof ((bp_t *)0)->raw_instr)

#define insert_before0(type, pos, ...)                                                \
  do {                                                                                \
    type *_p = malloc(sizeof(type));                                                  \
    *_p = (type){.NO = ++nr_breakpoints, .prev = pos->prev, .next = pos, __VA_ARGS__}; \
    pos->prev->next = _p;                                                             \
    pos->prev = _p;                                                                   \
  } while (0);

#define FOR_BREAKPOINTS(bp) \
  for (bp_t *bp = bp_nil->next; bp != bp_nil; bp = bp->next)

#define FOR_WATCHPOINTS(wp) \
  for (wp_t *wp = wp_nil->next; wp != wp_nil; wp = wp->next)

int create_breakpoint(char *e) {
  Elf_Addr addr = elf_find_func_byname(e);
  uint64_t raw_instr = 0;
  if (ELF_ADDR_VALID(addr)) {
    bool duplicate = false;
    FOR_BREAKPOINTS(bp) {
      if (bp->addr == addr) {
        if (!duplicate) {
          duplicate = true;
          raw_instr = bp->raw_instr;
          printf("Note: breakpoint %u", bp->NO);
        } else {
          printf(", %u", bp->NO);
        }
      }
    }
    if (duplicate) {
      printf(" also set at pc 0x%lx\n", (long) addr);
    } else {
      if (!in_pmem(addr)) {
        printf("Cannot insert breakpoint at 0x%lx\n", (long) addr);
        return 0;
      }
      raw_instr = host_read(guest_to_host(addr), LEN_BREAK_INST);
    }
    insert_before0(bp_t, bp_nil, .addr = addr, .duplicate = duplicate, .raw_instr = raw_instr);
    return nr_breakpoints;
  }
  return 0;
}

int create_watchpoint(char *e) {
  size_t nr_rpn;
  rpn_t *rpn = exprcomp(e, &nr_rpn);
  if (rpn == NULL) return 0;
  eval_t ev = eval(rpn, nr_rpn);
  insert_before0(wp_t, wp_nil, .expr = savestring(e), .nr_rpn = nr_rpn, .rpn = rpn,
                 .old_value = ev);
  return 0;
}

void disable_breakpoints() {
  FOR_BREAKPOINTS(bp) {
    if (!bp->duplicate)
      host_write(guest_to_host(bp->addr), LEN_BREAK_INST, bp->raw_instr);
  }
}

bool watchpoints_notify() {
  FOR_WATCHPOINTS(wp) {
    eval_t ev = eval(wp->rpn, wp->nr_rpn);
    if(!eveq(ev, wp->old_value))
      return 1;
  }
  return 0;
}

void enable_breakpoints() {
  FOR_BREAKPOINTS(bp) {
    if (!bp->duplicate)
      host_write(guest_to_host(bp->addr), LEN_BREAK_INST, breakpoint_instruction);
  }
}

static inline void free_bp(bp_t *p) {
  // XXX All breakpoints should be disabled ???
  int size = sizeof ((bp_t *)0)->raw_instr;
  assert(host_read(guest_to_host(p->addr), size) == p->raw_instr);
}

static inline void free_wp(wp_t *p) {
  free(p->expr);
  free(p->rpn);
}

static inline void delete_bp(bp_t *p) {
  free_bp(p);
  if (p->duplicate) return;
  FOR_BREAKPOINTS(bp) {
    if (bp->duplicate && bp->addr == p->addr) {
      bp->duplicate = false;
      break;
    }
  }
}

#define find_delete(loop, t, ...) \
  loop(t) {                       \
    if ((t)->NO == n) {           \
      __VA_ARGS__;                \
      goto found;                 \
    }                             \
    if ((t)->NO > n) break;       \
  }

bool delete_breakpoint(int n) {
  find_delete(FOR_BREAKPOINTS, t, delete_bp(t));
  find_delete(FOR_WATCHPOINTS, t, free_wp(t));
  return false;
found:
  return true;
}

void print_breakpoints() {
  FOR_BREAKPOINTS(bp) {
    printf("breakpoint %u at " FMT_PADDR "\n", bp->NO, bp->addr);
  }
}
void print_watchpoints() {
  FOR_WATCHPOINTS(wp) {
    printf("watchpoint %u, is `%s'\n", wp->NO, wp->expr);
  }
}

#define SORTED_FOR_ALL(bp, wp, flag)                                                 \
  for (bp = bp_nil->next, wp = wp_nil->next, flag = bp->NO < wp->NO; \
       bp != bp_nil || wp != wp_nil;                                                 \
       (flag = bp->NO < wp->NO) ? (bp = bp->next, 0) : (wp = wp->next, 0))

void init_breakpoints() {
  bp_nil = malloc(sizeof(bp_t));
  bp_nil->prev = bp_nil->next = bp_nil;
  wp_nil = malloc(sizeof(wp_t));
  wp_nil->prev = wp_nil->next = wp_nil;
  bp_nil->NO = wp_nil->NO = UINT_MAX;
  nr_breakpoints = 0;
}

void free_all_breakpoints() {
  bp_t *bp;
  wp_t *wp;
  int flag;
  SORTED_FOR_ALL(bp, wp, flag) {
    if (flag)
      free_bp(bp);
    else
      free_wp(wp);
  }
}
