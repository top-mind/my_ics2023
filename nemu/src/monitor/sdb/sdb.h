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
  eval_state state;
} eval_t;

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;
  rpn_t *rpn;
  size_t nr_rpn;
  eval_t old_value;
  char *hint;
  size_t hit;
} WP;

eval_t expr(char *e);
rpn_t *exprcomp(char *e, size_t *);
rpn_t *exprcomp_r(char *e, size_t *);
eval_t eval(const rpn_t *, size_t);
void peval(eval_t);


int new_wp(char *hint);
bool wp_delete(int n);
void print_wp();

#endif
