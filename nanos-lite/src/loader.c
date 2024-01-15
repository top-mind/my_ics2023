#include <proc.h>
#include <elf.h>
#include <fs.h>

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

static uintptr_t loader(PCB *pcb, const char *filename) {
  Elf_Ehdr ehdr;
  int fd = fs_open(filename, 0, 0);
  fs_read(fd, &ehdr, sizeof ehdr);
  assert(memcmp(ehdr.e_ident, ELFMAG, SELFMAG) == 0);
  assert(ehdr.e_type == ET_EXEC);
  assert(ehdr.e_machine == EXPECT_VALUE);
  assert(ehdr.e_version == EV_CURRENT);
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
      fs_read(fd, (void *)phdr.p_vaddr, phdr.p_filesz);
      memset((void *)(phdr.p_vaddr + phdr.p_filesz), 0, phdr.p_memsz - phdr.p_filesz);
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
  uintptr_t entry = loader(pcb, filename);
  // int argc, envc;
  void *usp;
  pcb->cp = ucontext(&pcb->as, (Area){pcb, pcb + 1}, (void *)entry);
  usp = new_page(NR_PAGE) + NR_PAGE * PGSIZE;
  // for (argc = 0; argv[argc]; argc++) {
  //   usp -= strlen(argv[argc]) + 1;
  //   strcpy(usp, argv[argc]);
  // }
  // for (envc = 0; envp[envc]; envc++) {
  //   usp -= strlen(envp[envc]) + 1;
  //   strcpy(usp, envp[envc]);
  // }
  // usp = (void *)ROUNDDOWN(usp, sizeof(void *));
  // for (int i = envc; i >= 0; i--) {
  //   usp -= sizeof(char *);
  //   *(char **)usp = envp[i];
  // }
  // for (int i = argc; i >= 0; i--) {
  //   usp -= sizeof(char *);
  //   *(char **)usp = argv[i];
  // }
  // usp -= sizeof(uintptr_t);
  // *(uintptr_t *)usp = argc;
  pcb->cp->GPRx = (uintptr_t)usp;
}
