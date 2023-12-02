#include "memory/host.h"
#include "memory/paddr.h"
#include "sdb.h"

bp_t *head_bp;
wp_t *head_wp;

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

int create_breakpoint(char *e) {
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
#define delete0(type, target)                        \
  ({                                                 \
    type *tmp = (type *)target->next;                \
    target->next = tmp == target ? NULL : tmp->next; \
    (void *)tmp;                                     \
  })

#define find_in_list(type, head)                 \
  do {                                           \
    if (head == NULL) { break; }                 \
    type *cur = head;                            \
    do {                                         \
      if (cur->next->NO == n) {                  \
        concat(free_, type)(delete0(type, cur)); \
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
}
static inline void print_watchpoint(wp_t *wp) {
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
}
