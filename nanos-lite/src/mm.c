#include <memory.h>
#include <proc.h>

static void *pf = NULL;

void* new_page(size_t nr_page) {
  void *p = pf;
  pf += nr_page * PGSIZE;
  if (!IN_RANGE(pf, heap)) {
    panic("out of memory: p = %p, nr_page = %d, reach to %p\n", p, nr_page, pf);
  }
  return p;
}

#ifdef HAS_VME
static void* pg_alloc(int n) {
  void *p = new_page(ROUNDUP(n, PGSIZE) / PGSIZE);
  memset(p, 0, n);
  return p;
}
#endif

void free_page(void *p) {
  // panic("not implement yet");
}

/* The brk() system call handler. */
int mm_brk(uintptr_t brk){
  if (current->max_brk < brk) {
    int nr_page = ROUNDUP(brk - current->max_brk, PGSIZE) / PGSIZE;
    void *p = new_page(nr_page);
    for (int i = 0; i < nr_page; i++) {
      map(&current->as, (void *)(current->max_brk + i * PGSIZE), p + i * PGSIZE, MMAP_READ | MMAP_WRITE);
    }
    current->max_brk = ROUNDUP(brk, PGSIZE);
  }
  return 0;
}

void init_mm() {
  pf = (void *)ROUNDUP(heap.start, PGSIZE);
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}
