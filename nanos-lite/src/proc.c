#include <proc.h>

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

void switch_boot_pcb() {
  current = &pcb_boot;
}

void hello_fun(void *arg) {
  yield();
  int j = 1;
  while (1) {
    Log("Hello World from Nanos-lite with arg '%p' for the %dth time!", (uintptr_t)arg, j);
    j ++;
    yield();
  }
}

void init_proc() {
  Log("Initializing processes...");
  context_kload(&pcb[0], (void *)hello_fun, (void *)0x12345678);
  context_uload(&pcb[1], "/bin/pal", (char *const[]){"/bin/pal", "--skip", NULL}, (char *const[]){NULL});
  // context_uload(&pcb[1], "/bin/hello", (char *const[]){"/bin/hello", "a", "b" , NULL}, (char *const[]){"a=x", "b=y", NULL});
  switch_boot_pcb();
}

Context* schedule(Context *prev) {
  current->cp = prev;
  current = (current == &pcb[0] ? &pcb[1] : &pcb[0]);
  return current->cp;
}
