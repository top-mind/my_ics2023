/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <readline/readline.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// #define TST_SPACES

// this should be enough
static char buf[65536] = {};
#define ARRLEN(x) (sizeof(x) / sizeof(x[0]))
static char code_buf[ARRLEN(buf) + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

uint32_t choose(uint32_t x) {
  static int has_srand = 0;
  if (!has_srand) {
    srand(time(NULL));
    has_srand = 1;
  }
  return rand() % x;
}

int nr_buf = 0;

void gen_num() {
  if (nr_buf > ARRLEN(buf) - 23)
    return;
  int neg = choose(5);
  for (int i = 0; i < neg; i++) {
    nr_buf += sprintf(buf + nr_buf, " -");
  }
  uint32_t num = (uint32_t) rand() | (rand() % 2 * (1u << 31));
  nr_buf += sprintf(buf + nr_buf, "%uu", num);
}

void gen(char x) {
  if (nr_buf >= ARRLEN(buf) - 1)
    return;
  buf[nr_buf++] = x;
}

void gen_rand_op() {
  if (nr_buf >= ARRLEN(buf) - 2)
    return;
  static const char k_op[] = "+\0"
                             "-\0"
                             "*\0"
                             "&\0"
                             "^\0"
                             "&\0"
                             "|\0";
  static const int k_op_idx[] = {0, 2, 4, 6, 8, 10, 12};
  int tmp = choose(ARRLEN(k_op_idx));
  nr_buf += sprintf(buf + nr_buf, "%s", k_op + k_op_idx[tmp]);
}

void gen_spaces() {
#ifdef TST_SPACES
  uint32_t num = choose(3);
  if (nr_buf >= ARRLEN(buf) - num)
    return;
  nr_buf += sprintf(buf + nr_buf, "%*.s", num, "");
#endif
}

void gen_rand_expr() {
  switch (choose(3)) {
    case 0: gen_num(); break;
     case 1: gen_spaces(); gen('('); gen_spaces(); gen_rand_expr(); gen_spaces(); gen(')'); gen_spaces(); break;
     default: gen_spaces(); gen_rand_expr(); gen_spaces(); gen_rand_op(); gen_spaces(); gen_rand_expr(); gen_spaces(); break;
  }
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop;) {
    nr_buf = 0;
    gen_rand_expr();
    buf[nr_buf] = '\0';

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc -Wall -Werror -Wno-parentheses /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;
    i++;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u ", result);
    for (int i = 0; i < nr_buf; i++) {
      if (buf[i] != 'u')
        putchar(buf[i]);
    }
    puts("");
  }
  return 0;
}
