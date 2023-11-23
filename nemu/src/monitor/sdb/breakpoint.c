#include "sdb.h"
#define NR_BP 32
static int head, end;
BP bp_pool[NR_BP];

int breakat(void *dummy) {
  head = 1, end = 1;
  return 0;
}
