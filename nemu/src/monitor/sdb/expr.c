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
* * See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "memory/paddr.h"
#include <isa.h>
#include <errno.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

#define PRIORITY(x) ((x.type == '+' || x.type == '-') ? 1 : 2)
#define UNARY (1 << 9)
#define ISBOP(x) \
  (x.type == '+' || x.type == '-' || x.type == '*' || x.type == '/' || x.type == TK_EQ)
#define ISUOP(x)  (x.type & UNARY)
#define ISOP(x)   (ISUOP(x) || ISBOP(x))
#define ISATOM(x) (x.type == TK_DECIMAL)

enum {
  TK_NOTYPE = 256,
  TK_EQ,
  TK_DECIMAL,
  TK_NEGTIVE = '-' | UNARY,
  TK_DEREF = '*' | UNARY
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
  {"[0-9]", TK_DECIMAL},
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
    word_t numconstant;
    const word_t *preg;
    int lbmatch;
    int save_last_lbrace;
  };
  int position;
} Token;

typedef struct {
  int type;
  union {
    word_t numconstant;
    const word_t *preg;
  };
} rpn_t;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;
static rpn_t g_rpn[ARRLEN(tokens)];

// Shared value for compile_token recurrence
// reversed_polish_notation
static rpn_t *p_rpn;
static int nr_rpn;
// rpn length limit
static int nr_rpn_limit;
// the user
static char *p_expr;

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

        if (rules[i].token_type == TK_NOTYPE) break;

        if (nr_token >= ARRLEN(tokens)) {
          puts("Expression too long (non-space tokens).");
          return false;
        }

        switch (tokens[nr_token].type = rules[i].token_type) {
          case TK_DECIMAL: {
            char *endptr;
            tokens[nr_token].numconstant = (word_t)strtoull(substr_start, &endptr, 10);

            substr_len = endptr - substr_start;
            position = endptr - e;

            if (errno == ERANGE) {
              puts("Numeric constant too large.");
              return 0;
            }
          } break;
          case '(':
            tokens[nr_token].save_last_lbrace = last_lbrace;
            last_lbrace = nr_token;
            break;
          case ')':
            if (last_lbrace == -1) {
              printf("Unbalanced right brace\n%s\n%*s^\n", e, position - substr_len, "");
              return false;
            }
            tokens[nr_token].lbmatch = last_lbrace;
            last_lbrace = tokens[last_lbrace].save_last_lbrace;
            break;
          case '-':
          case '*':
            if (nr_token == 0 || ISOP(tokens[nr_token - 1]))
              tokens[nr_token].type |= UNARY;
            break;
          default:;
        }

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
  if (l > r) {
    puts("Syntax error l > r");
    return 0;
  }
  if (nr_rpn >= nr_rpn_limit) {
    puts("Expression too long (atoms and operators in stack).");
    return 0;
  }
  if (l == r) {
    if (tokens[l].type == TK_DECIMAL) {
      p_rpn[nr_rpn].type = tokens[l].type;
      p_rpn[nr_rpn].numconstant = tokens[l].numconstant;
      nr_rpn++;
    } else {
      printf("Syntax error near `%s'\n", l + 1 < nr_token ? p_expr + tokens[l + 1].position : "");
      return 0;
    }
  } else {
    if (tokens[r].type == ')' && tokens[r].lbmatch == l)
      return compile_token(l + 1, r - 1);
    // find the operator with lowest priority
    int op_idx = -1;
    for (int i = r; i >= l; i = tokens[i].type == ')' ? tokens[i].lbmatch - 1 : i - 1) {
      if (!ISOP(tokens[i])) continue;
      if (op_idx == -1 || PRIORITY(tokens[i]) < PRIORITY(tokens[op_idx])) op_idx = i;
    }
    // no binary op, must be unary op
    if (op_idx == -1) {
      printf("Syntax error near `%s'\n", l + 1 < nr_token ? p_expr + tokens[l + 1].position : "");
      return 0;
    }
    int res1 = ISBOP(tokens[op_idx]) ? compile_token(l, op_idx - 1) : 1,
        res2 = compile_token(op_idx + 1, r);
    if (!res1 || !res2) return 0;
    if (nr_rpn >= nr_rpn_limit) {
      puts("Expression too long (atoms and operators in stack).");
      return 0;
    }
    p_rpn[nr_rpn++].type = tokens[op_idx].type;
  }
  return nr_rpn;
}

size_t exprcomp(char *e, rpn_t *rpn, size_t _rpn_length) {
  p_expr = e;
  if (!make_token(e) || nr_token == 0) return 0;
  p_rpn = rpn;
  nr_rpn = 0;
  nr_rpn_limit = _rpn_length;
  return compile_token(0, nr_token - 1);
}

typedef enum { EV_SUC, EV_DIVZERO, EV_INVADDR } eval_state;

typedef struct {
  word_t value;
  eval_state state;
} eval_t;

eval_t eval(rpn_t *p_rpn, size_t nr_rpn) {
  word_t *stack = (word_t *)malloc(sizeof(word_t) * nr_rpn);
  size_t i, nr_stk = 0;
  for (i = 0; i < nr_rpn; i++) {
    word_t src1, src2, res;
    if (ISOP(p_rpn[i])) src1 = stack[--nr_stk];
    if (ISBOP(p_rpn[i])) src2 = stack[--nr_stk];
    switch (p_rpn[i].type) {
      case '+': res = src1 + src2; break;
      case '-': res = src1 - src2; break;
      case '*': res = src1 * src2; break;
      case '/':
        if (src1 == 0) {
          free(stack);
          return (eval_t){0, EV_DIVZERO};
        }
        else
          res = src2 / src1;
        break;
      case TK_EQ: res = src1 == src2; break;
      case TK_DEREF:
        if (in_pmem(src1) && in_pmem(src1 + sizeof res - 1)) {
          res = paddr_read(src1, sizeof res);
        } else {
          free(stack);
          return (eval_t){src1, EV_INVADDR};
        }
        break;
      case TK_NEGTIVE:
        res = -src1;
        break;
      case TK_DECIMAL:
        res = p_rpn[i].numconstant;
        break;
      default: Assert(0, "operator %d not dealt with", p_rpn[i].type);
    }
    stack[nr_stk++] = res;
  }
  // XXX debug
  Assert(nr_stk == 1, "eval");
  word_t _value = stack[0];
  free(stack);
  return (eval_t){_value, EV_SUC};
}

word_t expr(char *e, bool *success) {
  Assert(e, REPORTBUG);
  if (!exprcomp(e, g_rpn, ARRLEN(g_rpn))) {
    *success = false;
    return 0;
  }

  // XXX debug
  for (int i = 0; i < nr_rpn; i++) {
    printf("%d %" PRIu32 "\n", g_rpn[i].type, g_rpn[i].numconstant);
  }
  // XXX gubed
  eval_t res = eval(g_rpn, nr_rpn);
  switch (res.state) {
    case EV_SUC: break;
    case EV_DIVZERO: puts("Division by zero"); break;
    case EV_INVADDR: printf("Cannot access memory at address " FMT_PADDR "\n", res.value); break;
    default: Assert(0, REPORTBUG);
  }
  *success = res.state == EV_SUC;
  return res.value;
}
