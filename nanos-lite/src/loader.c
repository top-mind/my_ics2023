#include <sys/errno.h>
#include <proc.h>
#include <elf.h>
#include <fs.h>
#include <stdint.h>

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
  assert(pcb);
  assert(pcb->as.ptr);
  Elf_Ehdr ehdr;
  int fd = fs_open(filename, 0, 0);
  if(fd < 0) {
    nanos_errno = ENOENT;
    return 0;
  }
  fs_read(fd, &ehdr, sizeof ehdr);
  if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0
      || ehdr.e_type != ET_EXEC
      || ehdr.e_machine != EXPECT_VALUE
      || ehdr.e_version != EV_CURRENT) {
    nanos_errno = ENOEXEC;
    return 0;
  }
  uint32_t nr_ph = ehdr.e_phnum;
  if (nr_ph == PN_XNUM) {
    Elf_Shdr shdr;
    fs_lseek(fd, ehdr.e_shoff, SEEK_SET);
    fs_read(fd, &shdr, sizeof shdr);
    nr_ph = shdr.sh_info;
  }
  for (uint32_t i = 0; i < nr_ph; i ++) {
    Elf_Phdr phdr;
    fs_lseek(fd, ehdr.e_phoff + i * sizeof phdr, SEEK_SET);
    fs_read(fd, &phdr, sizeof phdr);
    if (phdr.p_type == PT_LOAD) {
      fs_lseek(fd, phdr.p_offset, SEEK_SET);
      // phdr.p_offset, phdr.p_filesz
      // phdr.p_vaddr, phdr.p_memsz
      // fs_read(fd, (void *)phdr.p_vaddr, phdr.p_filesz);
      // memset((void *)(phdr.p_vaddr + phdr.p_filesz), 0, phdr.p_memsz - phdr.p_filesz);
    }
  }
  return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", entry);
  ((void(*)())entry) ();
}

#define NR_PAGE 8
void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]) {
  int argc, envc;
  void *page = new_page(NR_PAGE);
  void *sp = page + NR_PAGE * PGSIZE;
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
  // loader may destroy pcb, must be called last
  uintptr_t entry = loader(pcb, filename);
  if (!entry) {
    void free_page(void *);
    free_page(page);
    pcb->cp = NULL;
    return;
  }
  pcb->cp = ucontext(&pcb->as, (Area){pcb, pcb + 1}, (void *)entry);
  pcb->cp->GPRx = (uintptr_t)p;
}
