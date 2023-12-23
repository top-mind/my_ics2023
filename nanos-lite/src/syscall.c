#include <common.h>
#include <fs.h>
#include "sys/errno.h"
#include "syscall.h"
#include <sys/time.h>

void do_syscall(Context *c) {
  uintptr_t a[4]; a[0] = c->GPR1; a[1] = c->GPR2; a[2] = c->GPR3;
  a[3] = c->GPR4;

  switch (a[0]) {
    case SYS_exit: {
      void *naive_uload(void *pcb, const char *filename);
      Log("Halt system with exit code = %d", a[1]);
      naive_uload(NULL, "/bin/menu");
      break;
    }
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
    case SYS_lseek:
      c->GPRx = fs_lseek(a[1], a[2], a[3]);
      break;
    case SYS_gettimeofday:
      if (a[1] != 0) {
        uint64_t us = io_read(AM_TIMER_UPTIME).us;
        struct timeval *tv = (struct timeval *) a[1];
        tv->tv_sec = us / 1000000;
        tv->tv_usec = us % 1000000;
      }
      c->GPRx = 0;
      break;
    case SYS_execve: {
      void *naive_uload(void * pcb, const char *filename);
      int fd = fs_open((char *)a[1], 0, 0);
      if (fd < 0) {
        c->GPRx = -ENOEXEC;
        break;
      }
      naive_uload(NULL, (char *)a[1]);
      c->GPRx = -EACCES;
      break;
    }
    default: panic("Unhandled syscall ID = %d", a[0]);
  }
}
