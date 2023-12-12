#include <common.h>
#include "syscall.h"
void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;
  a[1] = c->GPR2;
  a[2] = c->GPR3;
  a[3] = c->GPR4;

  switch (a[0]) {
    case SYS_exit:
      Log("Halt system with exit code = %d", a[1]);
      halt(a[1]);
      break;
    case SYS_yield:
      Log("Yield syscall");
      c->GPRx = 0;
      break;
    case SYS_write:
      Log("Write syscall with fd = %d, buf = %p, len = %d", a[1], a[2], a[3]);
      if (a[1] == 1 || a[1] == 2) {
        for (size_t i = 0; i < a[3]; i++)
          putch(*(char *)(a[2] + i));
        c->GPRx = 0;
      } else {
        c->GPRx = -1;
        TODO();
      }
      break;
    case SYS_brk:
      Log("brk syscall with addr = %p", a[1]);
      c->GPRx = -1;
      break;
    default: panic("Unhandled syscall ID = %d", a[0]);
  }
}
