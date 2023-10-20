// TEST onlu
#ifndef TEST_H_
#define TEST_H_
#include <common.h>

static char _s[65536 + 5];

extern word_t expr(char *e, bool *success);

int test_main() {
  uint32_t ans;
  nemu_state.state = NEMU_QUIT;
  while (2 == scanf("%u%s", &ans, _s)) {
    bool suc = false;
    uint32_t out = expr(_s, &suc);
    if (!suc || ans != out) {
      printf("ans=%u,out=%u,expr=`%s'\n", ans, out, _s);
      nemu_state.state = NEMU_ABORT;
    }
  }
  return 0;
}
#endif
