#include <elf.h>
#include <errno.h>
#include <stdio.h>
#include <common.h>
#include <stdlib.h>
#include "sdb.h"

#define EM_AVAILABLE EM_RISCV

#define _DEF_ELF_TYPES(type, N) typedef Elf##N##_##type Elf_##type;

#ifdef ELF64
#define DEF_ELF_TYPES(type) _DEF_ELF_TYPES(type, 64)
#else
#define DEF_ELF_TYPES(type) _DEF_ELF_TYPES(type, 32)
#endif

#define ELF_TYPES(_) _(Ehdr) _(Shdr) _(Sym)
#define ELFCLASS     MUXDEF(ELF64, ELFCLASS64, ELFCLASS32)
#define ELF_ST_TYPE  ELF32_ST_TYPE

#define section_fseek(i) fseek(f, ehdr.e_shoff + sizeof(shdr) * (i), SEEK_SET)
#define R(x)             assert(1 == fread(&x, sizeof(x), 1, f))

MAP(ELF_TYPES, DEF_ELF_TYPES)

typedef struct {
  uintN_t addr;
  uintN_t size;
  char *name;
} func;

static func funcs[100];
static size_t nr_func;
static char *unknown_func = "??";

static int compfunc(const void *a, const void *b) {
  uintN_t addra = ((func *)a)->addr;
  uintN_t addrb = ((func *)b)->addr;
  return addra == addrb ? 0 : addra < addrb ? -1 : 1;
}

void init_addelf(char *filename) {
  FILE *f = fopen(filename, "r");
  if (f == NULL) {
    fprintf(stderr, ANSI_FMT("Unable to open `%s':\n%s\n", ANSI_FG_RED), filename, strerror(errno));
    return;
  }
  Elf_Ehdr ehdr;
  R(ehdr);
  Assert(ehdr.e_ident[EI_CLASS] == ELFCLASS, "Bad elf");
  Assert(ehdr.e_machine == EM_AVAILABLE, "Bad elf");

  uintN_t nr_sh = ehdr.e_shnum;
  Elf_Shdr shdr;
  section_fseek(0);
  R(shdr);
  if (shdr.sh_size > nr_sh) nr_sh = shdr.sh_size;
  for (uintN_t sh_idx = 1; sh_idx < nr_sh; sh_idx++) {
    section_fseek(sh_idx);
    R(shdr);
    if (shdr.sh_type == SHT_SYMTAB) {
      // save sh_offset and sh_size
      // read sh[sh_link]
      // save strtab_off[]
      // iterate from 0 to sh_size with steps sizeof(Elf_Sym)
      // for STT_FUNC
      //   name, value, size
      // for STN_ABS
      //   file
      uintN_t sh_off = shdr.sh_offset, sh_size = shdr.sh_size, str_size;
      section_fseek(shdr.sh_link);
      R(shdr);
      char *strtab = malloc(str_size = shdr.sh_size);
      fseek(f, shdr.sh_offset, SEEK_SET);
      Assert(1 == fread(strtab, str_size, 1, f), "Bad elf");
      Elf_Sym sym;
      for (uintN_t _off = 0; _off < sh_size; _off += sizeof(sym)) {
        fseek(f, sh_off + _off, SEEK_SET);
        R(sym);
        if (ELF_ST_TYPE(sym.st_info) == STT_FUNC) {
          if (nr_func == ARRLEN(funcs)) {
            printf("Too many functions\n");
            goto _break;
          }
          funcs[nr_func++] = (func){
            .addr = sym.st_value,
            .size = sym.st_size,
            .name = savestring(&strtab[sym.st_name]),
          };
        }
      }
      free(strtab);
    }
  }
  _break:
  qsort(funcs, nr_func, sizeof(func), compfunc);
  printf("Readed %zu symbols from %s\n", nr_func, filename);
  for (int i = 0; i < nr_func; i++)
    printf("name='%s', %#lx, %ld\n", funcs[i].name, (long)funcs[i].addr, (long)funcs[i].size);
  fclose(f);
}

void elf_getname_and_offset(uintN_t addr, char **name, uintN_t *offset) {
  uintN_t uhole;
  char *nhole;
  if (name == NULL) name = &nhole;
  if (offset == NULL) offset = &uhole;
  for (int i = 0; i < nr_func; i++) {
    if (funcs[i].addr <= addr && addr < funcs[i].addr + funcs[i].size) {
      *name = funcs[i].name;
      *offset = addr - funcs[i].addr;
      return;
    }
  }
  *name = unknown_func;
  *offset = -1;
}
