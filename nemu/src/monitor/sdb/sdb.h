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

// elf
#include <elf-def.h>

// expr
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

eval_t expr(char *e);
rpn_t *exprcomp(char *e, size_t *);
eval_t eval(const rpn_t *, size_t);
void peval(eval_t);
static inline bool eveq(eval_t a, eval_t b) {
  return a.type == b.type && a.value == b.value;
}

// breakpoints
void init_breakpoints();
int create_breakpoint(char *e);
int create_watchpoint(char *e);

bool delete_breakpoint(int n);
bool delete_watchpoint(int n);

void print_all_breakpoints();
void print_watchpoints();

void enable_breakpoints();
void disable_breakpoints();

#define breakpoint_instruction \
  MUXDEF(CONFIG_ISA_x86, 0xcc, \
  MUXDEF(CONFIG_ISA_mips32, 0x0005000d, \
  MUXDEF(CONFIG_ISA_riscv, 0x00100073, )))
#endif
