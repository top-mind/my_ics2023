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

#include <isa.h>
#include <errno.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256,
  TK_EQ,
  TK_DECIMAL,
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  {" +", TK_NOTYPE}, // spaces
  {"\\+", '+'},      // plus
  {"-", '-'},        // minus
  {"\\*", '*'},      // times
  {"/", '/'},        // divide
  {"==", TK_EQ},     // equal
  {"0|[1-9][0-9]*", TK_DECIMAL},
  {"\\(", '('}, // lbrace
  {"\\)", ')'}, // rbrace
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

// static int verbose = 0;

typedef struct token {
  int type;
  union {
    char str[32];
    int lbmatch;
    int save_last_lbrace;
  } data;
  int position;
} Token;
#define TOKEN_STR_LIMIT ARRLEN(((Token *)NULL)->data.str)
typedef struct {
  int type;
  word_t data;
} rpn_t;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;
static rpn_t g_rpn[ARRLEN(tokens)];

// Shared value for recurrence function compile_token
// So the functions are not thread safe.
static rpn_t *prpn;
static int nr_rpn;
// rpn length limit
static int rpn_length;
// the user 
static char *g_expression;


static bool make_token(char *e) {
  int position = 0;
  int i;
  int last_lbrace = -1;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex,
            position, substr_len, substr_len, substr_start);

        position += substr_len;

        if (rules[i].token_type == TK_NOTYPE)
          break;

        if (nr_token >= ARRLEN(tokens)) {
          puts("Expression too long");
          return false;
        }

        switch (rules[i].token_type) {
          case TK_DECIMAL:
            // tokens[nr_token].str = substr_len >
            if (substr_len >= TOKEN_STR_LIMIT) {
              printf("'%.*s...' exceeds length limit of %d.\n", TOKEN_STR_LIMIT - 1,
                     substr_start, TOKEN_STR_LIMIT);
              return false;
            }
            memcpy(tokens[nr_token].data.str, substr_start, substr_len);
            tokens[nr_token].data.str[substr_len] = '\0';
            printf("'%s' saved\n", tokens[nr_token].data.str);
            break;
          case '(':
            tokens[nr_token].data.save_last_lbrace = last_lbrace;
            last_lbrace = nr_token;
            break;
          case ')':
            if (last_lbrace == -1) {
              printf("Unbalanced right brace\n%s\n%.*s^\n", e, position - substr_len, "");
              return false;
            }
            tokens[nr_token].data.lbmatch = last_lbrace;
            last_lbrace = tokens[last_lbrace].data.save_last_lbrace;
            break;
          default:;
        }
        tokens[nr_token].type = rules[i].token_type;
        tokens[nr_token].position = position - substr_len;
        nr_token++;
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  if (last_lbrace != -1) {
    printf("Unbalanced left brace\n%s\n%*s^\n", e, tokens[last_lbrace].position, "");
    return false;
  }
  return true;
}

// 把tokens编译成逆波兰表达式 (reverse polish notation)
// return: 表达式符号数
static int compile_token(int l, int r) {
  return 1;
  if (l > r) {
    puts("Syntex error l>r");
    return 0;
  }
  if (l == r) {
    if (tokens[l].type == TK_DECIMAL) {
    } else {
      return 0;
    }
  }
  return nr_rpn;
}

size_t exprcomp(char *e, rpn_t *rpn, size_t _rpn_length) {
  g_expression = e;
  if (!make_token(e) || nr_token == 0)
    return false;
  prpn = rpn;
  rpn_length = _rpn_length;
  return compile_token(0, nr_token - 1);
}

typedef enum { EV_SUC, EV_DIVZERO, EV_INVADDR } eval_state;

typedef struct {
  word_t value;
  eval_state state;
  size_t position;
} eval_t;

eval_t eval() {
  Assert(0, "Internal assertion error. " REPORTBUG);
  return (eval_t){0, EV_SUC};
}

word_t expr(char *e, bool *success) {
  Assert(e, REPORTBUG);
  if (!exprcomp(e, g_rpn, ARRLEN(g_rpn))) {
    *success = false;
    return 0;
  }
  *success = true;
  return 0xdeadbeaf;
}
