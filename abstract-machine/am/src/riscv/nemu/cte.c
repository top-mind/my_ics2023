#include <am.h>
#include <riscv/riscv.h>
#include <klib.h>

#ifdef __ISA_RISCV32__
#define MCAUSE_INT 0x80000000
#define IRQ_TIMER 0x80000007
#elif defined(__ISA_RISCV64__)
#define MCAUSE_INT 0x8000000000000000
#define IRQ_TIMER 0x8000000000000007
#endif

static Context* (*user_handler)(Event, Context*) = NULL;

void __am_get_cur_as(Context *c);
void __am_switch(Context *c);

Context *__am_irq_handle(Context *c) {
  __am_get_cur_as(c);
  if (user_handler) {
    Event ev = {0};
    switch (c->mcause) {
    case IRQ_TIMER:
      ev.event = EVENT_IRQ_TIMER;
      break;
    case 8:
    case 11:
      if (c->GPR1 == -1)
        ev.event = EVENT_YIELD;
      else
        ev.event = EVENT_SYSCALL;
      break;
    default:
      ev.event = EVENT_ERROR;
      break;
    }
    if (!(c->mcause & MCAUSE_INT))
      c->mepc += 4;
    c = user_handler(ev, c);
    assert(c != NULL);
  }
  __am_switch(c);
  return c;
}

extern void __am_asm_trap(void);

bool cte_init(Context *(*handler)(Event, Context *)) {
  // initialize exception entry
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));

  // register event handler
  user_handler = handler;

  return true;
}

Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
  Context *c = (Context *)kstack.end - 1;
  c->gpr[2] = (uintptr_t)c;
  c->mepc = (uintptr_t)entry;
  c->mstatus = 0x1880;
  c->GPR2 = (uintptr_t)arg;
  c->pdir = NULL;
  return c;
}

void yield() {
#ifdef __riscv_e
  asm volatile("li a5, -1; ecall");
#else
  asm volatile("li a7, -1; ecall");
#endif
}

bool ienabled() { return false; }

void iset(bool enable) {}
