#include <elf-def.h>
#include <memory/host.h>
#include <memory/paddr.h>
#include "sdb.h"
#include "utils.h"
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

static bool breakpoint_enable;

#define LEN_BREAK_INST (sizeof((bp_t *)0)->raw_instr)

#define insert_before0(type, pos, ...)                                                 \
  ({                                                                                   \
    type *_p = malloc(sizeof(type));                                                   \
    *_p = (type){.NO = ++nr_breakpoints, .prev = pos->prev, .next = pos, __VA_ARGS__}; \
    pos->prev->next = _p;                                                              \
    pos->prev = _p;                                                                    \
    _p;                                                                                \
  })

#define FOR_BREAKPOINTS(bp) for (bp_t *bp = bp_nil->next; bp != bp_nil; bp = bp->next)

#define FOR_WATCHPOINTS(wp) for (wp_t *wp = wp_nil->next; wp != wp_nil; wp = wp->next)

static void print_breakpoint(bp_t *bp) {
  printf("breakpoint %u at " FMT_PADDR "\n", bp->NO, bp->addr);
}

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
      printf(" also set at pc 0x%lx\n", (long)addr);
    } else {
      if (!in_pmem(addr)) {
        printf("Cannot insert breakpoint at 0x%lx\n", (long)addr);
        return 0;
      }
      raw_instr = host_read(guest_to_host(addr), LEN_BREAK_INST);
    }
    bp_t *p =
      insert_before0(bp_t, bp_nil, .addr = addr, .duplicate = duplicate, .raw_instr = raw_instr);
    print_breakpoint(p);
    return nr_breakpoints;
  } else {
    printf("%s is not a function name\n", e);
  }
  return 0;
}

int create_watchpoint(char *e) {
  size_t nr_rpn;
  rpn_t *rpn = exprcomp(e, &nr_rpn);
  if (rpn == NULL) {
    puts("");
    return 0;
  }
  eval_t ev = eval(rpn, nr_rpn);
  insert_before0(wp_t, wp_nil, .expr = savestring(e), .nr_rpn = nr_rpn, .rpn = rpn,
                 .old_value = ev);
  return nr_breakpoints;
}

// TODO ???
// See free_bp()
void disable_breakpoints() {
  breakpoint_enable = false;
  FOR_BREAKPOINTS(bp) {
    if (!bp->duplicate &&
        /* TODO */
        host_read(guest_to_host(bp->addr), LEN_BREAK_INST) == breakpoint_instruction)
      host_write(guest_to_host(bp->addr), LEN_BREAK_INST, bp->raw_instr);
  }
}

void watchpoints_notify() {
  if (nemu_state.state > NEMU_STOP) return;
  FOR_WATCHPOINTS(wp) {
    eval_t ev = eval(wp->rpn, wp->nr_rpn);
    if (!eveq(ev, wp->old_value)) {
      nemu_state.state = NEMU_STOP;
      printf("\nWatchpoint %u: %s\nOld = ", wp->NO, wp->expr);
      peval(wp->old_value);
      printf("\nNew = ");
      peval(ev);
      puts("");
      wp->old_value = ev;
    }
  }
}

void enable_breakpoints() {
  breakpoint_enable = 1;
  FOR_BREAKPOINTS(bp) {
    if (!bp->duplicate) host_write(guest_to_host(bp->addr), LEN_BREAK_INST, breakpoint_instruction);
  }
}

static inline void free_bp(bp_t *p) {
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

#define find_delete(loop, t, ...)  \
  loop(t) {                        \
    if ((t)->NO == n) {            \
      (t)->prev->next = (t)->next; \
      (t)->next->prev = (t)->prev; \
      __VA_ARGS__;                 \
      goto found;                  \
    }                              \
    if ((t)->NO > n) break;        \
  }

bool delete_breakpoint(int n) {
  find_delete(FOR_BREAKPOINTS, t, delete_bp(t));
  find_delete(FOR_WATCHPOINTS, t, free_wp(t));
  return false;
found:
  return true;
}

#define SORTED_FOR_ALL(bp, wp, flag)                                                               \
  for (bp = bp_nil->next, wp = wp_nil->next, flag = bp->NO < wp->NO; bp != bp_nil || wp != wp_nil; \
       flag ? (void)(bp = bp->next):(wp = wp->next), flag = bp->NO < wp->NO)

static inline void print_watchpoint(wp_t *wp) {
  printf("watchpoint %u `%s'\n", wp->NO, wp->expr);
}

void print_all_breakpoints() {
  bp_t *bp;
  wp_t *wp;
  bool f;
  SORTED_FOR_ALL(bp, wp, f) {
    if (f)
      print_breakpoint(bp);
    else
      print_watchpoint(wp);
  }
}

void print_watchpoints() {
  FOR_WATCHPOINTS(wp) { print_watchpoint(wp); }
}

bool breakpoint_notify(vaddr_t pc) {
  if (!breakpoint_enable) return 0;
  FOR_BREAKPOINTS(bp) {
    if (bp->addr == pc) {
      printf("hit breakpoint %u at " FMT_PADDR "\n", bp->NO, bp->addr);
      return true;
    }
  }
  return false;
}

void init_breakpoints() {
  bp_nil = malloc(sizeof(bp_t));
  wp_nil = malloc(sizeof(wp_t));
  bp_nil->prev = bp_nil->next = bp_nil;
  wp_nil->prev = wp_nil->next = wp_nil;
  bp_nil->NO = wp_nil->NO = UINT_MAX;
  nr_breakpoints = 0;
}

void free_all_breakpoints() {
  assert(!breakpoint_enable);
  // TODO
  FOR_BREAKPOINTS(p) {
    int size = sizeof((bp_t *)0)->raw_instr;
    if (host_read(guest_to_host(p->addr), size) != p->raw_instr) {
      fprintf(stderr,
              ANSI_FMT("Warning: Breakpoint at " FMT_PADDR
                       " collapsed: user program take it over.\n",
                       ANSI_FG_RED),
              p->addr);
    }
  }
  for (bp_t *p = bp_nil->next, *q; p != bp_nil; p = q) {
    q = p->next;
    free_bp(p);
    free(p);
  }
  for (wp_t *p = wp_nil->next, *q; p != wp_nil; p = q) {
    q = p->next;
    free_wp(p);
    free(p);
  }
  bp_nil->prev = bp_nil->next = bp_nil;
  wp_nil->prev = wp_nil->next = wp_nil;
}
