#include <proc.h>

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

void switch_boot_pcb() {
  current = &pcb_boot;
}

void hello_fun(void *arg) {
  int j = 1;
  while (1) {
    if ((j % 60) == 1)
      Log("Hello World from Nanos-lite with arg '%p' for the %dth time!", (uintptr_t)arg, j);
    j ++;
    yield();
  }
}

void init_proc() {
  Log("Initializing processes...");
  context_uload(&pcb[0], "/bin/hello", (char *const[]){"hello", NULL}, (char *const[]){NULL});
  context_uload(&pcb[1], "/bin/nterm", (char *const[]){"/bin/nterm", NULL}, (char *const[]){NULL});
  context_uload(&pcb[2], "/bin/pal", (char *const[]){"/bin/pal", "--skip", NULL}, (char *const[]){NULL});
  context_uload(&pcb[3], "/bin/menu", (char *const[]){"/bin/menu", NULL}, (char *const[]){NULL});
  switch_boot_pcb();
}

PCB *fg_pcb = &pcb[1];

Context* schedule(Context *prev) {
  // Log("prev: %p", prev);
  current->cp = prev;
  // current = (current == &pcb[0] ? &pcb[1] : &pcb[0]);
  if (current == &pcb[0]) {
    current = fg_pcb;
  } else {
    current = &pcb[0];
  }
  return current->cp;
}

void switch_fg(int i) {
  fg_pcb = &pcb[i];
}
