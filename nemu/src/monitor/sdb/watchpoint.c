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
  while (head != NULL)
    wp_delete(head->NO);
}

/* assgin a watchpoint
 * return the NO. of the assigned watchpoint
 * if failed, return -1
 */
int new_wp(char *hint) {
  WP *r = free_;
  if (r != NULL) {
    r->rpn = exprcomp_dynamic(hint, &r->nr_rpn);
    if (r->rpn == NULL)
      return -1;
    r->old_value = eval(r->rpn, r->nr_rpn);
    r->hint = (char *)malloc(strlen(hint) + 1);
    strcpy(r->hint, hint);
    free_ = r->next;
    r->next = head;
    head = r;
  }
  return r ? r->NO : -1;
}

static void free_wp(WP *wp) {
  free(wp->rpn);
  free(wp->hint);
  wp->next = free_;
  free_ = wp;
}

bool wp_delete(int n) {
  if (head == NULL)
    return false;
  WP *wp;
  if (head->NO == n) {
    wp = head;
    head = head->next;
  } else {
    WP *tmp = head;
    while (tmp->next && tmp->next->NO != n)
      tmp = tmp->next;
    wp = tmp->next;
    if (wp == NULL)
      return false;
    tmp->next = wp->next;
  }
  free_wp(wp);
  return true;
}
