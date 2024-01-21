#include <sys/errno.h>
#include <proc.h>
#include <elf.h>
#include <fs.h>
#include <stdint.h>
#include <memory.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
# define Elf_Shdr Elf64_Shdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
# define Elf_Shdr Elf32_Shdr
#endif

#if defined (__ISA_AM_NATIVE__)
# define EXPECT_VALUE EM_X86_64
#elif defined(__ISA_X86__)
# define EXPECT_VALUE EM_386
#elif defined(__ISA_MIPS32__)
# define EXPECT_VALUE EM_MIPS
#elif defined(__riscv)
# define EXPECT_VALUE EM_RISCV
#else
#error Unsupported ISA
#endif

void context_kload(PCB *pcb, void (*entry)(void *), void *arg) {
  pcb->cp = kcontext((Area){pcb, pcb + 1}, entry, arg);
}

int nanos_errno;

static uintptr_t loader(PCB *pcb, const char *filename) {
  pcb->max_brk = 0;
  Elf_Ehdr ehdr;
  int fd = fs_open(filename, 0, 0);
  if(fd < 0) {
    nanos_errno = ENOENT;
    Log("fs_open '%s' return with %d", fd);
    goto out;
  }
  fs_read(fd, &ehdr, sizeof ehdr);
  if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0
      || ehdr.e_type != ET_EXEC
      || ehdr.e_machine != EXPECT_VALUE
      || ehdr.e_version != EV_CURRENT) {
    nanos_errno = ENOEXEC;
    goto out;
  }
  uint32_t nr_ph = ehdr.e_phnum;
  if (nr_ph == PN_XNUM) {
    Elf_Shdr shdr;
    fs_lseek(fd, ehdr.e_shoff, SEEK_SET);
    fs_read(fd, &shdr, sizeof shdr);
    nr_ph = shdr.sh_info;
  }
  if (nr_ph * sizeof(Elf_Phdr) > PGSIZE) {
    Log("Program header size too large (nr_ph = %d, file = %s)", nr_ph, filename);
    nanos_errno = ENOEXEC;
    goto out;
  }
  Elf_Phdr *phdr = (Elf_Phdr *)new_page(1);
  fs_lseek(fd, ehdr.e_phoff, SEEK_SET);
  fs_read(fd, phdr, nr_ph * sizeof(Elf_Phdr));
  for (int i = 0; i < nr_ph; i++) {
    if (phdr[i].p_type != PT_LOAD) continue;
    if (phdr[i].p_memsz == 0) continue;
    if (phdr[i].p_filesz > phdr[i].p_memsz) {
      nanos_errno = ENOEXEC;
      goto out_free;
    }
    if (phdr[i].p_align < 12) {
      Log("Invalid alignment (p_align = %d, file = %s)", phdr[i].p_align, filename);
      nanos_errno = ENOEXEC;
      goto out_free;
    }
    uintptr_t va = phdr[i].p_vaddr;
    uintptr_t va_hi = ROUNDDOWN(va, PGSIZE);
    uintptr_t va_lo = va & (PGSIZE - 1);
    uintptr_t va_end = va + phdr[i].p_memsz;
    int nr_page = (ROUNDUP(va_end, PGSIZE) - ROUNDDOWN(va, PGSIZE)) / PGSIZE;
    void *page = new_page(nr_page);
    fs_lseek(fd, phdr[i].p_offset, SEEK_SET);
    fs_read(fd, page + va_lo, phdr[i].p_filesz);
    if (phdr[i].p_memsz > phdr[i].p_filesz) {
      memset(page + va_lo + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
    }
    int prot = (phdr[i].p_flags & PF_R ? MMAP_READ : 0) |
               (phdr[i].p_flags & PF_W ? MMAP_WRITE : 0);
    for (int j = 0; j < nr_page; j++)
      map(&pcb->as, (void *)va_hi + j * PGSIZE, page + j * PGSIZE, prot);
    if (pcb->max_brk < va_end) pcb->max_brk = va_end;
  }
  pcb->max_brk = ROUNDUP(pcb->max_brk, PGSIZE);
  free_page(phdr);
  return ehdr.e_entry;
out_free:
  free_page(phdr);
out:
  return 0;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", entry);
  ((void(*)())entry) ();
}

#define NR_USTACKPG 8
void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]) {
  protect(&pcb->as);
  uintptr_t entry = loader(pcb, filename);
  if (!entry) {
    pcb->cp = NULL;
    return;
  }
  int argc, envc;
  void *page = new_page(NR_USTACKPG);
  void *sp = page + NR_USTACKPG * PGSIZE;
  for (envc = 0; envp[envc]; envc++);
  for (int i = envc - 1; i >= 0; i--) {
    sp -= strlen(envp[i]) + 1;
    strcpy(sp, envp[i]);
  }
  for (argc = 0; argv[argc]; argc++);
  for (int i = argc - 1; i >= 0; i--) {
    sp -= strlen(argv[i]) + 1;
    strcpy(sp, argv[i]);
  }
  uintptr_t *p = (uintptr_t *)ROUNDDOWN(
      sp - sizeof(void *) * (1 + argc + 1 + envc + 1), sizeof(void *));
  p[0] = (uintptr_t)argc;
  for (int i = 0; i < argc; i++) {
    p[i + 1] = (uintptr_t)sp;
    sp += strlen(argv[i]) + 1;
  }
  p[argc + 1] = 0;
  for (int i = 0; i < envc; i++) {
    p[argc + 2 + i] = (uintptr_t)sp;
    sp += strlen(envp[i]) + 1;
  }
  p[argc + 2 + envc] = 0;
  void *vpage = pcb->as.area.end - NR_USTACKPG * PGSIZE;
  for (int i = 0; i < NR_USTACKPG; i++) {
    map(&pcb->as, vpage + PGSIZE * i,
        page + PGSIZE * i, MMAP_WRITE);
  }
  pcb->cp = ucontext(&pcb->as, (Area){pcb, pcb + 1}, (void *)entry);
  pcb->cp->GPRx = vpage + (uintptr_t)p - page;
}
