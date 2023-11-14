// TEST onlu
#ifndef TEST_H_
#define TEST_H_
#include <common.h>

static char _s[65536 + 25];

typedef enum { EV_SUC, EV_DIVZERO, EV_INVADDR, EV_SYNTAX } eval_state;

typedef struct eval_t {
  word_t value;
  eval_state state;
} eval_t;
extern eval_t expr(char *e, bool *success);

int test_main() {
  puts("Warning: TEST MODE!");
  uint32_t ans;
  nemu_state.state = NEMU_QUIT;
  int nr_ok = 0;
  // while (scanf("%u%s", &ans, _s) == 2) {
  //   size_t len = strlen(_s);
  //   uint32_t out;
  //   sprintf(_s + len, "==%u", ans);
  //   bool suc;
  //   if ((out = expr(_s, &suc)) != 1) {
  //     printf("ans=%u,out=%u,expr=`%s'\n", ans, out, _s);
  //     nemu_state.state = NEMU_ABORT;
  //   } else {
  //     nr_ok ++;
  //   }
  // }
  while (scanf("%u", &ans) == 1) {
    if (!fgets(_s, ARRLEN(_s), stdin)) break;
    if (_s[strlen(_s) - 1] != '\n') continue;
    _s[strlen(_s) - 1] = '\0';
    uint32_t out;
    bool suc;
    if ((out = expr(_s, &suc).state) != ans) {
      printf("ans=%u,out=%u,expr=`%s'\n", ans, out, _s);
      nemu_state.state = NEMU_ABORT;
    } else {
      nr_ok ++;
    }
  }
  printf("%d\n", nr_ok);
  return 0;
}
#endif
