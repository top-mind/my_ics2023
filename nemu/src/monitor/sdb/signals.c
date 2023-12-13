#include <signal.h>
#include <stddef.h>

int sync_sigint;

void sigint_handler(int sig) { sync_sigint = 1; }

void init_sigint() { signal(SIGINT, sigint_handler); }
