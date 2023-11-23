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

#ifndef __SDB_H__
#define __SDB_H__

#include <cpu/decode.h>
#include <common.h>
#include <elf-def.h>
typedef struct {
  int type;
  union {
    word_t numconstant;
    const word_t *preg;
  };
} rpn_t;

typedef enum { EV_SUC, EV_DIVZERO, EV_INVADDR, EV_SYNTAX } eval_state;

typedef struct eval_t {
  word_t value;
  eval_state type;
} eval_t;

typedef struct _watchpoint{
  int NO;
  rpn_t *rpn;
  size_t nr_rpn;
  eval_t old_value;
  char *hint;
  size_t hit;
  struct _watchpoint *next;
} WP;

eval_t expr(char *e);
rpn_t *exprcomp(char *e, size_t *);
rpn_t *exprcomp_r(char *e, size_t *);
eval_t eval(const rpn_t *, size_t);
void peval(eval_t);


int new_wp(char *hint);
bool wp_delete(int n);
void print_wp();

typedef struct {
} watchpoint;

typedef struct __attribute__((packed)) _breakpoint {
  int num, hit, next, prev;
  char *hint;
  bool is_watchpoint;
  union {
    struct {
      paddr_t addr;
      MUXDEF(CONFIG_ISA_x86, uint8_t, uint32_t) raw_instr;
    } b;
    struct {
      rpn_t *rpn;
      size_t nr_rpn;
      eval_t old_value;
    } w;
  };
} BP;

#endif
