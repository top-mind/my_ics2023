#include <memory.h>

static void *pf = NULL;

void* new_page(size_t nr_page) {
  void *p = pf;
  pf += nr_page * PGSIZE;
  return p;
}

#ifdef HAS_VME
static void* pg_alloc(int n) {
  void *p = new_page(ROUNDUP(n, PGSIZE) / PGSIZE);
  printf("pg_alloc %p %d\n", p, n);
  memset(p, 0, n);
  return p;
}
#endif

void free_page(void *p) {
  // panic("not implement yet");
}

/* The brk() system call handler. */
int mm_brk(uintptr_t brk) {
  return 0;
}

void init_mm() {
  pf = (void *)ROUNDUP(heap.start, PGSIZE);
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}
