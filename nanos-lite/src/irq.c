#include <common.h>

void do_syscall(Context *c);

Context *schedule(Context *prev);

void update_keybrd();

static Context* do_event(Event e, Context* c) {
  switch (e.event) {
    case EVENT_PAGEFAULT:
      Log("EVENT_PAGEFAULT hanging for debug");
      while (1);
      return c;
    case EVENT_IRQ_TIMER:
      return schedule(c);
    case EVENT_IRQ_IODEV:
      // Log("EVENT_IRQ_IODEV");
      update_keybrd();
      return c;
    case EVENT_YIELD:
      return schedule(c);
      break;
    case EVENT_SYSCALL:
      do_syscall(c);
      break;
    case EVENT_ERROR:
      panic("Event error");
      return c;
    default: panic("Unhandled event ID = %d", e.event);
  }

  return c;
}

void init_irq(void) {
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}
