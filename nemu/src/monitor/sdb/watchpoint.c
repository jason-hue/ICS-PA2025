/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
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
  char expr[128];
  word_t old_val;
  word_t new_val;
  /* TODO: Add more members if necessary */
} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP *new_wp();
void free_wp(WP *wp);

WP *new_wp()
{
  assert(free_ != NULL && "No free watch point available!");

  WP *temp = free_;
  free_ = free_->next;
  temp->next = head;
  head = temp;
  return temp;
}
// 将WP节点放回空闲链表头部
void free_wp(WP *wp)
{
  assert(wp != NULL && "wp is NULL when free_wp!");
  if (head == wp)
  {
    head = head->next;
  }
  else
  {
    WP *temp = head;
    while (temp != NULL && temp->next != wp)
    {
      temp = temp->next;
    }
    if (temp != NULL) {
      temp->next = wp->next;
    }
  }
  wp->next = free_;
  free_ = wp;
}
int set_watchpoint(char *e)
{
  WP *wp = new_wp();
  bool success = true;
  word_t val = expr(e,&success);
  if (!success) {
    printf("Invalid expression: %s\n", e);
    return -1;
  }
  strcpy(wp->expr, e);
  wp->old_val = val;
  wp->new_val = val;
  printf("Watchpoint %d: %s\n", wp->NO, wp->expr);
  return wp->NO;
}
