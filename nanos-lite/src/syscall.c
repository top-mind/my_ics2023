#include <common.h>
#include <fs.h>
#include "sys/errno.h"
#include "syscall.h"
#include <stdlib.h>
#include <sys/time.h>
#include <proc.h>

void do_syscall(Context *c) {
  uintptr_t a[4]; a[0] = c->GPR1; a[1] = c->GPR2; a[2] = c->GPR3;
  a[3] = c->GPR4;

  switch (a[0]) {
    case SYS_exit: {
      context_uload(current, "/bin/menu", (char *const[]){"/bin/menu", NULL}, (char *[]){NULL});
      switch_boot_pcb();
      yield();
      assert(0);
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
    case SYS_execve:
      context_uload(current, (char *) a[1], (char **) a[2], (char **) a[3]);
      if (current->cp == NULL) {
        c->GPRx = -EACCES;
      } else {
        printf("execve: '%s'\n", (char *) a[1]);
        // TODO free page
        switch_boot_pcb();
        yield();
        assert(0);
      }
      break;
    default: panic("Unhandled syscall ID = %d", a[0]);
  }
}
