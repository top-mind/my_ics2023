#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef __ISA_NATIVE__
#error can not support ISA=native
#endif

#define SYS_yield 1
extern int _syscall_(int, uintptr_t, uintptr_t, uintptr_t);

int main() {
  struct timeval a;
  printf("%d\n", sizeof a.tv_usec);
  return 0;
}
