#include "memory/host.h"
#include "memory/paddr.h"
#include "sdb.h"
#include "elf-def.h"

bp_t *head_bp;
wp_t *head_wp;
size_t nr_breakpoints;

static void *breakpoint_next(bool next, bool *type) {
  static bp_t *cur_bp = NULL;
  static wp_t *cur_wp = NULL;
  if (!next) {
    cur_bp = head_bp;
    cur_wp = head_wp;
    return NULL;
  } else {
    if (cur_bp == NULL && cur_wp == NULL) {
      return NULL;
    }
    //                    true                false       false         true
    *type = cur_bp == NULL ? 1 : cur_wp == NULL ? 0: cur_bp->NO > cur_wp->NO;
    if (*type) {
      void *ret = cur_wp;
      cur_wp = cur_wp->next;
      return ret;
    } else {
      void *ret = cur_bp;
      cur_bp = cur_bp->next;
      return ret;
    }
  }
}

#define create0(type, head, ...) \
  ({                        \
   type *old = malloc(sizeof(type)); \
   type *target; \
   if (head == NULL) target = head = old; else target = head, *old = *head; \
   *head = (type){.NO = ++nr_breakpoints, .next = old, __VA_ARGS__}; \
   head = old; \
   target; \
  })
int create_breakpoint(char *e) {
  uintN_t addr = elf_getaddr(e);
  if (ELF_OFFSET_VALID(addr)) {
    // test if already exists
    bool duplicate = false;
    bp_t *cur = head_bp;
    do {
      if (cur->addr == addr) {
        duplicate = true;
        break;
      }
    } while(cur != head_bp);
    bp_t *t = create0(bp_t, head_bp, .addr = addr, .duplicate = duplicate);
    if (!duplicate) {
      t->raw_instr = paddr_read(addr, sizeof ((bp_t *)0)->raw_instr);
    }
    return nr_breakpoints;
  } else {
    printf("No symbol %s in elf file(s)", e);
  }
  return 0;
}

int create_watchpoint(char *e) {
  rpn_t *rpn;
  size_t nr_rpn;
  if ((rpn = exprcomp(e, &nr_rpn)) == NULL)
    return 0;
  create0(wp_t, head_wp, .expr = savestring(e), .rpn = rpn, .nr_rpn = nr_rpn,
          .old_value = eval(rpn, nr_rpn));
  return nr_breakpoints;
}

static inline void free_bp_t(void *e) {}

static inline void free_wp_t(wp_t *e) {
  free(e->expr);
  free(e->rpn);
}

bool delete_breakpoint(int n) {
#define delete0(type, head, target)                        \
  ({                                                 \
    type *tmp = (type *)target->next;                \
    target->next = tmp == target ? NULL : tmp->next; \
    if (head == tmp) head = target->next;         \
    (void *)tmp;                                     \
  })

#define find_in_list(type, head)                 \
  do {                                           \
    if (head == NULL) { break; }                 \
    type *cur = head;                            \
    do {                                         \
      if (cur->next->NO == n) {                  \
        concat(free_, type)(delete0(type, head, cur)); \
        return 1;                                \
      }                                          \
    } while ((cur = cur->next) != head);         \
  } while (0)

  find_in_list(bp_t, head_bp);
  find_in_list(wp_t, head_wp);
  return 0;
#undef delete0
#undef find_in_list
}

static inline void print_breakpoint(bp_t *bp) {
  printf("breakpoint %d at " FMT_PADDR "\n", bp->NO, bp->addr);
}
static inline void print_watchpoint(wp_t *wp) {
  printf("watchpoint %d, is `%s'\n", wp->NO, wp->expr);
}

void print_all_breakpoints() {
  breakpoint_next(0, NULL);
  bool type;
  void *cur;
  while ((cur = breakpoint_next(1, &type)) != NULL) {
    if (type == false)
      print_breakpoint(cur);
    else
      print_watchpoint(cur);
  }
}

void print_watchpoints() {
  wp_t *cur = head_wp;
  do {
    print_watchpoint(cur);
  } while ((cur = cur->next) != head_wp);
}

void free_all_breakpoints() {
  breakpoint_next(0, NULL);
  bool type;
  void *cur;
  while ((cur = breakpoint_next(1, &type)) != NULL) {
    if (type == false)
      free_bp_t(cur);
    else
      free_wp_t(cur);
  }
  head_bp = NULL;
  head_wp = NULL;
}
