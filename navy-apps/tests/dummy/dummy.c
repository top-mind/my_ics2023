#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <fixedptc.h>

#ifdef __ISA_NATIVE__
#error can not support ISA=native
#endif

#define SYS_yield 1
extern int _syscall_(int, uintptr_t, uintptr_t, uintptr_t);

int main() {
  int32_t volatile b = ((fixedpt)((1.5) * ((fixedpt)((fixedpt)1 << (32 - 24))) + 0.5));
  return 0;
}
