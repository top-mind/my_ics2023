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
#include "sdb.h"
#include <isa.h>
#include <errno.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

/* priority and associativity base on ~/Downloads/priority.jpg
 deref neg	10	right to left
 * / %		9
 + -		8	ok
 << >>		7
 == !=		6	ok
 &		5
 ^		4
 |		3
 &&		2	ok
 ||		1
 atom		0	ok
 */

// bit 7:0	ascii code
// bit 8	alternative code
// bit 12:9	priority
#define PRIO(x)     ((x) << 9)
#define PRIORITY(x) (((x.type) >> 9) & 0xf)
#define ALT         (1 << 8)
#define ISUOP(x)    (PRIORITY(x) == 0xf)
#define ISBOP(x)    (ISOP(x) && !ISUOP(x))
#define RTOL(x)     ISUOP(x)
#define ISOP(x)     (PRIORITY(x) > 0)
// clang-format off
enum {
  TK_NOTYPE,
  TK_NUM,
  TK_DOLLAR,
  TK_REF     = '&' | PRIO(15),
  TK_DEREF   = '*' | PRIO(15),
  TK_NEGTIVE = '-' | PRIO(15),
  TK_BITNOT  = '~' | PRIO(15),
  TK_NOT     = '!' | PRIO(15),
  TK_TIMES   = '*' | PRIO(10),
  TK_DIVIDE  = '/' | PRIO(10),
  TK_MOD     = '%' | PRIO(10),
  TK_PLUS    = '+' | PRIO(9),
  TK_MINUS   = '-' | PRIO(9),
  TK_LSHIFT  = '<' | PRIO(8),
  TK_RSHIFT  = '>' | PRIO(8),
  TK_LU      = '<' | PRIO(7),
  TK_GU      = '>' | PRIO(7),
  TK_LEQ     = '<' | PRIO(7) | ALT,
  TK_GEQ     = '>' | PRIO(7) | ALT,
  TK_EQ      = '=' | PRIO(6),
  TK_NEQ     = '!' | PRIO(6),
  TK_BITAND  = '&' | PRIO(5),
  TK_BITXOR  = '^' | PRIO(4),
  TK_BITOR   = '|' | PRIO(3),
  TK_AND     = '&' | PRIO(2),
  TK_OR      = '|' | PRIO(1),
};
// clang-format on

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  {" +", TK_NOTYPE},  // spaces
  {"\\*", TK_TIMES},  // times
  {"/", TK_DIVIDE},   // divide
  {"%", TK_MOD},      // modulos
  {"\\+", TK_PLUS},   // plus
  {"-", TK_MINUS},    // minus
  {"<<", TK_LSHIFT},  // left shift
  {"<=", TK_LEQ},     // less or equal
  {"<", TK_LU},       // less than
  {">>", TK_RSHIFT},  // right shift
  {">=", TK_GEQ},     // greater or equal
  {">", TK_GU},       // greater than
  {"~", TK_BITNOT},   // bit not
  {"\\^", TK_BITXOR}, // bit xor
  {"==", TK_EQ},      // equal
  {"!=", TK_NEQ},     // not equal
  {"!", TK_NOT},      // not
  {"&&", TK_AND},     // logical and
  {"&", TK_BITAND},   // bit and
  {"\\|\\|", TK_OR},  // logical or
  {"\\|", TK_BITOR},  // bit or
  {"[0-9]", TK_NUM},  // num
  {"\\(", '('},       // lbrace
  {"\\)", ')'},       // rbrace
  {"\\$[a-z0-9$]+", TK_DOLLAR},
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

typedef struct token {
  int type;
  union {
    word_t constant;
    const word_t *preg;
    int lbmatch;
    int save_last_lbrace;
  };
  int position;
} Token;

static Token tokens[65536] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;

// rpn is reversed polish notation
static rpn_t g_rpn[ARRLEN(tokens)];
static int nr_g_rpn;
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

        // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        if (rules[i].token_type == TK_NOTYPE) break;

        if (nr_token >= ARRLEN(tokens)) {
          printf("Expression too long (non-space tokens).");
          return false;
        }
        char *endptr;
        char save;
        word_t *preg;
        switch (tokens[nr_token].type = rules[i].token_type) {
          case TK_NUM:
            errno = 0;
            tokens[nr_token].constant = (word_t)strtoull(substr_start, &endptr, 0);
            if (errno == ERANGE) {
              printf("Numeric constant too large.");
              return false;
            }
            substr_len = endptr - substr_start;
            position = endptr - e;
            break;
          case TK_DOLLAR:
            save = substr_start[substr_len];
            substr_start[substr_len] = '\0';
            preg = isa_reg_str2ptr(substr_start + 1);
            if (preg == NULL) {
              printf("Invalid register name '%s'", substr_start + 1);
              return false;
            }
            tokens[nr_token].preg = preg;
            substr_start[substr_len] = save;
            break;
          case '(':
            tokens[nr_token].save_last_lbrace = last_lbrace;
            last_lbrace = nr_token;
            break;
          case ')':
            if (last_lbrace == -1) {
              printf("Unbalanced right brace\n%s\n%*s^", e, position - substr_len, "");
              return false;
            }
            tokens[nr_token].lbmatch = last_lbrace;
            last_lbrace = tokens[last_lbrace].save_last_lbrace;
            break;
          case TK_MINUS:
          case TK_TIMES:
            if (nr_token == 0 || ISOP(tokens[nr_token - 1]) || tokens[nr_token - 1].type == '(')
              tokens[nr_token].type |= PRIO(0xf);
            break;
          default:;
        }

        tokens[nr_token].position = position - substr_len;
        nr_token++;
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^", position, e, position, "");
      return false;
    }
  }
  if (last_lbrace != -1) {
    printf("Unbalanced left brace\n%s\n%*s^", e, tokens[last_lbrace].position, "");
    return false;
  }
  return true;
}

#define ESYNTAX(pos)                                                                       \
  do {                                                                                     \
    printf("Syntax error near `%s'", pos < nr_token ? p_expr + tokens[pos].position : ""); \
    return 0;                                                                              \
  } while (0)
#define ETOOLONG                                                   \
  do {                                                             \
    printf("Expression too long (atoms and operators in stack)."); \
    return 0;                                                      \
  } while (0)
/* Compile expression to reverse polish notation
 * return: array length
 * If failed, or the expression is empty, return 0
 */
static int compile_token(int l, int r) {
  if (l > r) ESYNTAX(l);
  if (nr_g_rpn >= ARRLEN(g_rpn)) ETOOLONG;
  if (l == r) {
    g_rpn[nr_g_rpn].type = tokens[l].type;
    switch (tokens[l].type) {
      case TK_NUM: g_rpn[nr_g_rpn].numconstant = tokens[l].constant; break;
      case TK_DOLLAR: g_rpn[nr_g_rpn].preg = tokens[l].preg; break;
      default: ESYNTAX(l + 1);
    }
    nr_g_rpn++;
  } else {
    if (tokens[r].type == ')' && tokens[r].lbmatch == l) return compile_token(l + 1, r - 1);
    // find the operator with lowest priority
    int op_idx = -1;
    for (int i = r; i >= l; i = tokens[i].type == ')' ? tokens[i].lbmatch - 1 : i - 1) {
      if (!ISOP(tokens[i])) continue;
      if (op_idx == -1 || PRIORITY(tokens[i]) < PRIORITY(tokens[op_idx]) ||
          (PRIORITY(tokens[i]) == PRIORITY(tokens[op_idx]) && RTOL(tokens[i])))
        op_idx = i;
    }
    if (op_idx == -1) ESYNTAX(l + 1);
    int lres = ISBOP(tokens[op_idx]) ? compile_token(l, op_idx - 1) : 1;
    int res = lres ? compile_token(op_idx + 1, r) : 0;
    if (!res) return 0;
    if (nr_g_rpn >= ARRLEN(g_rpn)) ETOOLONG;
    g_rpn[nr_g_rpn++].type = tokens[op_idx].type;
  }
  return nr_g_rpn;
}
#undef ESYNTAX
#undef ETOOLONG

/* Compile expression to reverse polish notation
 * return: rpn_t array
 * If failed, or the expression is empty, return NULL
 */
static rpn_t *exprcomp0(char *e, size_t *p_nr_rpn) {
  size_t null;
  if (p_nr_rpn == NULL) p_nr_rpn = &null;
  p_expr = e;
  if (!make_token(e) || nr_token == 0) {
    *p_nr_rpn = 0;
    return NULL;
  }
  nr_g_rpn = 0;
  *p_nr_rpn = nr_g_rpn = compile_token(0, nr_token - 1);
  return nr_g_rpn == 0 ? NULL : g_rpn;
}

/* Compile expression to reverse polish notation.
 * Return a copy of g_rpn.
 * If failed, or the expression is empty, return NULL
 * If you just want to evaluate the expression, use expr() instead.
 */
rpn_t *exprcomp(char *e, size_t *p_nr_rpn) {
  if (exprcomp0(e, p_nr_rpn) == NULL) return NULL;
  rpn_t *rpn = (rpn_t *)malloc(sizeof(rpn_t) * nr_g_rpn);
  memcpy(rpn, g_rpn, sizeof(rpn_t) * nr_g_rpn);
  return rpn;
}

/* eval(const rpn_t *p_rpn, size_t nr_rpn)
 * p_rpn      array of reverse polish notation with length
 * nr_rpn
 *   This function will use a stack to evaluate the compiled expression.
 * If any syntactically error occurs, nemu PANIC.
 * Return value
 *  A struct eval_t, to indicate the result of evaluation, or 0 if
 *  runtime error occurs. The error type is indicated by .state.
 * This function has no side effect.
 */
eval_t eval(const rpn_t *p_rpn, size_t nr_rpn) {
  word_t *stack = (word_t *)malloc(sizeof(word_t) * nr_rpn);
  size_t i, nr_stk = 0;
  for (i = 0; i < nr_rpn; i++) {
    word_t lsrc = 0, rsrc = 0, res;
    if (ISOP(p_rpn[i])) rsrc = stack[--nr_stk];
    if (ISBOP(p_rpn[i])) lsrc = stack[--nr_stk];
    switch (p_rpn[i].type) {
      case TK_PLUS: res = lsrc + rsrc; break;
      case TK_MINUS: res = lsrc - rsrc; break;
      case TK_TIMES: res = lsrc * rsrc; break;
      case TK_DIVIDE:
        if (rsrc == 0) {
          free(stack);
          return (eval_t){0, EV_DIVZERO};
        } else
          res = lsrc / rsrc;
        break;
      case TK_MOD:
        if (rsrc == 0) {
          free(stack);
          return (eval_t){0, EV_DIVZERO};
        } else
          res = lsrc % rsrc;
        break;
      case TK_LU: res = lsrc < rsrc; break;
      case TK_LEQ: res = lsrc <= rsrc; break;
      case TK_GU: res = lsrc > rsrc; break;
      case TK_GEQ: res = lsrc >= rsrc; break;
      case TK_BITNOT: res = ~rsrc; break;
      case TK_NOT: res = !rsrc; break;
      case TK_BITAND: res = lsrc & rsrc; break;
      case TK_BITOR: res = lsrc | rsrc; break;
      case TK_BITXOR: res = lsrc ^ rsrc; break;
      case TK_LSHIFT: res = lsrc << (rsrc & (sizeof res * 8 - 1)); break;
      case TK_RSHIFT: res = lsrc >> (rsrc & (sizeof res * 8 - 1)); break;
      case TK_EQ: res = lsrc == rsrc; break;
      case TK_NEQ: res = lsrc != rsrc; break;
      case TK_AND: res = lsrc && rsrc; break;
      case TK_OR: res = lsrc || rsrc; break;
      case TK_DEREF:
        if (in_pmem(rsrc) && in_pmem(rsrc + sizeof res - 1)) {
          res = paddr_read(rsrc, sizeof res);
        } else {
          free(stack);
          return (eval_t){rsrc, EV_INVADDR};
        }
        break;
      case TK_NEGTIVE: res = -rsrc; break;
      case TK_NUM: res = p_rpn[i].numconstant; break;
      case TK_DOLLAR: res = *p_rpn[i].preg; break;
      default: Assert(0, "operator %d not dealt with", p_rpn[i].type);
    }
    stack[nr_stk++] = res;
  }
  Assert(nr_stk == 1, "Fatal error, bad implement compile expression");
  word_t _value = stack[0];
  free(stack);
  return (eval_t){_value, EV_SUC};
}

/* Evaluate expression
 * This is a wrapper of exprcomp() and eval().
 */
eval_t expr(char *e) {
  Assert(e, REPORTBUG);
  if (!exprcomp0(e, NULL)) return (eval_t){0, EV_SYNTAX};
  return eval(g_rpn, nr_g_rpn);
}

/* nemu's info command require a no-newline output.
 */
void peval(eval_t ev) {
  switch (ev.type) {
    case EV_SUC: printf(FMT_WORD, ev.value); break;
    case EV_DIVZERO: printf("Division by zero"); break;
    case EV_INVADDR: printf("Cannot access address " FMT_PADDR, ev.value); break;
    case EV_SYNTAX: break;
  }
}
