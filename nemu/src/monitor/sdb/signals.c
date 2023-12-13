#include <signal.h>
#include <common.h>

int sync_sigint;

void sigint_handler(int sig) { 
  if (nemu_state.state == NEMU_RUNNING) {
    sync_sigint = 1;
  } else {
    exit(0);
  }
}

void init_sigint() { signal(SIGINT, sigint_handler); }
