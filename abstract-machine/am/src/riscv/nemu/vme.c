#include <am.h>
#include <nemu.h>
#include <klib.h>

static AddrSpace kas = {};
static void* (*pgalloc_usr)(int) = NULL;
static void (*pgfree_usr)(void*) = NULL;
static int vme_enable = 0;

static Area segments[] = {      // Kernel memory mappings
  NEMU_PADDR_SPACE
};

#define USER_SPACE RANGE(0x40000000, 0x80000000)

static inline void set_satp(void *pdir) {
  uintptr_t mode = 1ul << (__riscv_xlen - 1);
  asm volatile("csrw satp, %0" : : "r"(mode | ((uintptr_t)pdir >> 12)));
}

static inline uintptr_t get_satp() {
  uintptr_t satp;
  asm volatile("csrr %0, satp" : "=r"(satp));
  return satp << 12;
}

bool vme_init(void* (*pgalloc_f)(int), void (*pgfree_f)(void*)) {
  pgalloc_usr = pgalloc_f;
  pgfree_usr = pgfree_f;

  kas.ptr = pgalloc_f(PGSIZE);

  int i;
  for (i = 0; i < LENGTH(segments); i ++) {
    void *va = segments[i].start;
    for (; va < segments[i].end; va += PGSIZE) {
      map(&kas, va, va, 0);
    }
  }

  set_satp(kas.ptr);
  vme_enable = 1;

  return true;
}

void protect(AddrSpace *as) {
  PTE *updir = (PTE*)(pgalloc_usr(PGSIZE));
  as->ptr = updir;
  as->area = USER_SPACE;
  as->pgsize = PGSIZE;
  // map kernel space
  memcpy(updir, kas.ptr, PGSIZE);
}

void unprotect(AddrSpace *as) {
}

void __am_get_cur_as(Context *c) {
  c->pdir = (vme_enable ? (void *)get_satp() : NULL);
}

void __am_switch(Context *c) {
  if (vme_enable && c->pdir != NULL && c->pdir != kas.ptr) {
    set_satp(c->pdir);
  }
}

#define PAGE_SHIFT 12
#define PAGE_SIZE  (1ul << PAGE_SHIFT)
#define PAGE_MASK  (PAGE_SIZE - 1)

// prot is ignored
// DAgUXWR
void map(AddrSpace *as, void *va, void *pa, int prot) {
  assert(as);
  assert(as->ptr);
  assert((uintptr_t)va % PGSIZE == 0);
  assert((uintptr_t)pa % PGSIZE == 0);
  PTE *p = as->ptr;
  p += ((uintptr_t)va >> 22);

  PTE pte = PTE_V | PTE_R | PTE_W | PTE_X | PTE_U | PTE_A | PTE_D;
#ifndef __LP64__
  // Sv32 2-level page table
  PTE *pdir;
  if (!(*p & PTE_V)) {
    pdir = (PTE *)pgalloc_usr(PGSIZE);
    *p = ((uintptr_t)pdir >> 2) | PTE_V;
  } else {
    pdir = (PTE *)((*p << 2) & ~PAGE_MASK);
  }
  pdir[((uintptr_t)va >> 12) & 0x3ff] = (((uintptr_t)pa & ~PAGE_MASK) >> 2) | pte;
#else
#error "not implemented"
#endif
}

Context *ucontext(AddrSpace *as, Area kstack, void *entry) {
  Context *c = (Context *)kstack.end - 1;
  c->gpr[2] = (uintptr_t) c;
  c->mepc = (uintptr_t) entry;
  c->mstatus = 0xc0080;
  c->pdir = as->ptr;
  return c;
}
