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
#include <memory/paddr.h>
#include <memory/vaddr.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#define PRI(x) ((x) << 9)

enum {
  UNARY = 0x100,
  ATOM = 0x80, TK_NUM, TK_REG,
  /*
   * Pay attention to the precedence level of different rules.
   */
  TK_PLUS   = '+' | PRI(13),
  TK_DIVIDE = '/' | PRI(14),
  TK_LPAREN = '(',
  TK_RPAREN = ')' | ATOM,
  TK_EQ     = '=' | PRI(10),
  TK_NEQ    = '!' | PRI(10),
  TK_AND    = '&' | PRI(6),
  TK_DEREF  = '*' | UNARY | PRI(14),
  TK_NEG    = '-' | UNARY | PRI(13),
  TK_NOTYPE = 0,
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  {" +",                TK_NOTYPE},
  {"\\+",               TK_PLUS},
  {"-",                 TK_NEG},
  {"\\*",               TK_DEREF},
  {"/",                 TK_DIVIDE},
  {"\\(",               TK_LPAREN},
   {"\\)",              TK_RPAREN},
  {"[1-9][0-9]*"
   "|0[xX][0-9a-fA-F]+"
   "|0[0-7]*",          TK_NUM},
  {"\\$[[:alnum:]]+",   TK_REG},
  {"==",                TK_EQ},
  {"!=",                TK_NEQ},
  {"&&",                TK_AND},
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

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

#define NR_STR 32
typedef struct token {
  int type;
  int position; // For runtime syntax error location
  union {
    char str[NR_STR]; // register
    word_t num; // number
    int rparen_match; // for expr() check_parenthesis
    int lparen_next; // only for make_token()
  };
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static int g_lparen = -1;

static bool make_token(const char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;
  g_lparen = -1;

  if (e == NULL)
    return false;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0
          && pmatch.rm_so == 0) {
        const char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        position += substr_len;

        if (rules[i].token_type == TK_NOTYPE)
          break;
        if (nr_token >= ARRLEN(tokens)) {
          printf("Token limit exceeded at position %d\n%s\n%*.s^\n",
                 position - substr_len, e, position - substr_len, "");
          return false;
        }
        tokens[nr_token].type = rules[i].token_type;
        tokens[nr_token].position = position - substr_len;
        char *pend;
        switch (rules[i].token_type) {
          case TK_NUM:
            tokens[nr_token].num = (word_t) strtoull(substr_start, &pend, 0);
            Assert(pend - substr_start == substr_len,
                   "Regex for numerical value works abnormally, expect %s len=%d, found len=%zd",
                   substr_start, substr_len, pend - substr_start);
            break;
          /* Next two cases match pairs of '()'. */
          case TK_LPAREN:
            tokens[nr_token].lparen_next = g_lparen;
            g_lparen = nr_token;
            break;
          case TK_RPAREN:
            if (g_lparen == -1) {
              printf("Unbalanced parentheses at position %d\n%s\n%*.s^\n",
                     position - 1, e, position - 1, "");
              return false;
            }
            int tmp = g_lparen;
            g_lparen = tokens[g_lparen].lparen_next;
            tokens[tmp].rparen_match = nr_token;
            break;
          case TK_REG:
            if (substr_len > NR_STR) {
              printf("Token limit exceeded at position %d\n%s\n%*.s^\n",
                     position - substr_len + NR_STR - 1, e,
                     position - substr_len + NR_STR - 1, "");
              return false;
            }
            memcpy(tokens[nr_token].str, substr_start + 1, substr_len - 1);
            tokens[nr_token].str[substr_len - 1] = 0;
            bool success;
            isa_reg_str2val(tokens[nr_token].str, &success);
            if (success == false) {
              printf("%s is neither an ABI register name nor a register code\n",
                     tokens[nr_token].str);
              return false;
            }
            break;
          default:;
        }
        nr_token++;
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  if (g_lparen != -1) {
    printf("Unbalanced parentheses at position %d\n%s\n%*.s^\n",
           tokens[g_lparen].position, e, tokens[g_lparen].position, "");
    return false;
  }
  // First order token parse success
  // Now make unary operator
  for (int i = 0; i < nr_token; i++)
    if ((tokens[i].type & UNARY) && i != 0 && (tokens[i - 1].type & ATOM)) {
        tokens[i].type ^= UNARY;
    }
  return nr_token > 0;
}

typedef struct eval_t {
  enum {
    EV_SUC, EV_SYN, EV_FPE, EV_SEG,
  } errno;
  union {
    word_t value;
    int err_tok;
    struct {
      int seg_tok;
      vaddr_t err_addr;
    };
  };
} eval_t;

eval_t eval(int l, int r) {
  eval_t a = {.errno = 4, .value = 0};
  while (tokens[l].type == TK_LPAREN && tokens[l].rparen_match == r) {
    ++l;
    --r;
  }
  if (l > r) {
    a.errno = EV_SYN;
    a.err_tok = r;
    return a;
  }
  else if (l == r) {
    bool suc;
    switch (tokens[l].type) {
      case TK_NUM:
        a.value = tokens[l].num;
        a.errno = EV_SUC;
        break;
      case TK_REG:
        a.value = isa_reg_str2val(tokens[l].str, &suc);
        a.errno = EV_SUC;
        Assert(suc, "Fatal error in isa_reg_str2val: found '%s'\n",
               tokens[l].str);
        break;
      default:
        a.errno = EV_SYN;
        a.err_tok = l;
    }
    return a;
  }
  int op = -1;
  for (int i = l; i <= r; i++) { // condition i < r is ok, but it's hard to
                                 // trace where syntax error occurs
    if (tokens[i].type == TK_LPAREN) {
      i = tokens[i].rparen_match;
      continue;
    }
    if (!(((ATOM | UNARY) & tokens[i].type))
        && (op == -1
            || (tokens[op].type & PRI(15)) >= (tokens[i].type & PRI(15))))
      op = i;
  }

  if (op == -1) {
    switch(tokens[l].type) {
      case TK_NEG:
        a = eval(l + 1, r);
        if (a.errno == EV_SUC)
          a.value = -a.value;
        break;
      case TK_DEREF:
        a = eval(l + 1, r);
        if (a.errno == EV_SUC) {
          if (in_pmem((paddr_t)a.value)) {
            a.value = vaddr_read(a.value, sizeof(a.value));
          } else {
            a.errno = EV_SEG;
            a.err_addr = a.value;
            a.seg_tok = l;
          }
        }
        break;
      default:
        a.errno = EV_SYN;
        a.err_tok = l;
        break;
    }
    return a;
  }
  if (op == l || op == r) {
    a.errno = EV_SYN;
    a.err_tok = op;
    return a;
  }
  a = eval(l, op-1);
  if (a.errno != EV_SUC)
    return a;
  eval_t rres = eval(op + 1, r);
  if (rres.errno != EV_SUC)
    return rres;
  switch (tokens[op].type & 127) {
    case '+':
      a.value += rres.value;
      break;
    case '-':
      a.value -= rres.value;
      break;
    case '*':
      a.value *= rres.value;
      break;
    case '/':
      if (rres.value == 0) {
        a.errno = EV_FPE;
        a.err_tok = op;
      } else {
        a.value /= rres.value;
      }
      break;
    case '=':
      a.value = a.value == rres.value;
      break;
    case '!':
      a.value = a.value != rres.value;
      break;
    case '&':
      a.value = a.value && rres.value;
      break;
  }
  return a;
}

static void print_error(const char *e, eval_t a) {
  switch(a.errno) {
    case EV_SYN:
      printf("Syntax error");
      break;
    case EV_FPE:
      printf("Division by zero");
      break;
    case EV_SEG:
      printf("Invalid virtual address " FMT_WORD "\n%s\n%*.s^\n", a.err_addr, e,
             tokens[a.seg_tok].position, "");
      return;
    default:
      assert(0);
  }
  printf(" at position %d\n%s\n%*.s^\n", tokens[a.err_tok].position, e,
         tokens[a.err_tok].position, "");
}

#ifdef CONFIG_WATCHPOINT_STOP

word_t expr(const char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return EV_SYN;
  }
  eval_t a = eval(0, nr_token - 1);
  if (a.errno == EV_SUC) {
    *success = true;
    return a.value;
  }
  *success = false;
  print_error(e, a);
  return a.errno;
}

#else

word_t expr_r(const char *e, bool *success, bool verbose) {
  if (!make_token(e)) {
    *success = false;
    return EV_SYN;
  }
  eval_t a = eval(0, nr_token - 1);
  if (a.errno == EV_SUC) {
    *success = true;
    return a.value;
  }
  *success = false;
  if (verbose)
    print_error(e, a);
  return a.errno;
}

word_t expr(const char *e, bool *success) {
  return expr_r(e, success, true);
}

#endif
