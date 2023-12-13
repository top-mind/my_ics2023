#include <common.h>
#include <fs.h>
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
      c->GPRx = fs_write(a[1], (void *) a[2], a[3]);
      break;
    case SYS_brk:
      Log("brk syscall with addr = %p", a[1]);
      c->GPRx = 0;
      break;
    case SYS_read:
      c->GPRx = fs_read(a[1], (void *) a[2], a[3]);
      break;
    case SYS_open:
      c->GPRx = fs_open((char *) a[1], a[2], a[3]);
      break;
    case SYS_close:
      c->GPRx = 0;
      break;

    default: panic("Unhandled syscall ID = %d", a[0]);
  }
}
