#include <signal.h>
#include <common.h>
#include <stdatomic.h>
// 0: idle
// 1: running (allow interrupt)
// 2: interrupted
atomic_uchar a_nemu_state;

void sigint_handler(int sig) { 
  // 0 -> exit(0)
  // 1 -> 2
  if (atomic_load(&a_nemu_state) == 0) {
    exit(0);
  } else {
    atomic_store(&a_nemu_state, 2);
  }
}

void init_sigint() {
  atomic_init(&a_nemu_state, 0);
  signal(SIGINT, sigint_handler);
}
