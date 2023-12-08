// TEST onlu
#ifndef TEST_H_
#define TEST_H_
#ifdef CONFIG_TEST
#include <common.h>

static char _s[65536 + 25];

extern struct {
  word_t value;
  unsigned int st;
} expr(char *e);

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
    if ((out = expr(_s).value) != ans) {
      printf("ans=%u,out=%u,expr=`%s'\n", ans, out, _s);
      nemu_state.state = NEMU_ABORT;
    } else {
      nr_ok++;
    }
  }
  printf("%d\n", nr_ok);
  return 0;
}
#endif
#endif
