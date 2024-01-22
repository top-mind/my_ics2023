#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef __ISA_NATIVE__
#error can not support ISA=native
#endif

#define SYS_yield 1
extern int _syscall_(int, uintptr_t, uintptr_t, uintptr_t);

int main() {
  int i = 0;
  while (1) {
    printf("Hello, AM World @ %d!\n", i++);
    _syscall_(SYS_yield, 0, 0, 0);
  }
  return 0;
}
