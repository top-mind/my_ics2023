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

#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;
  rpn_t *rpn;
  eval_t old_value;
} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

bool wp_empty() {
  return head == NULL;
}

void wp_delete_all() {
  TODO();
}

WP *new_wp(rpn_t *rpn, size_t nr_rpn) {
  WP *ret = free_;
  if (free_ != NULL) {
    ret = free_;
    ret->next = head;
    ret->rpn = rpn;
    ret->old_value = eval(rpn, nr_rpn);
    free_ = ret->next;
  }
  return ret;
}

void free_wp(WP *wp) {
  if (head == wp) {
    head = wp->next;
  } else {
    WP *tmp = head;
    while (tmp->next && tmp->next != wp)
      tmp = tmp->next;
    tmp->next = wp->next;
  }
  wp->next = free_;
  free_ = wp;
}
