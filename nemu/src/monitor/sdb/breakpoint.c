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
  if (ELF_ADDR_VALID(addr)) {
    bool duplicate = false;
    FOR_BREAKPOINTS(bp) {
      if (bp->addr == addr) {
        duplicate = true;
        break;
      }
    }
    uint64_t raw_instr = 0;
    if (!duplicate) {
      if (!in_pmem(addr)) {
        printf("Cannot insert breakpoint at %lx\n", (long) addr);
        return 0;
      }
      int size = sizeof ((bp_t *)0)->raw_instr;
      raw_instr = host_read(guest_to_host(addr), size);
      host_write(guest_to_host(addr), size, breakpoint_instruction);
    }
    insert_before0(bp_t, bp_nil, .addr = addr, .duplicate = duplicate, .raw_instr = raw_instr);
  }
  return 0;
}

int create_watchpoint(char *e) {
  return 0;
}

static inline void free_bp_t(void *e) {}

static inline void free_wp_t(wp_t *e) {
  free(e->expr);
  free(e->rpn);
}

bool delete_breakpoint(int n) {
  return false;
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
      free_bp_t(bp);
    else
      free_wp_t(wp);
  }
}
