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
} Symbol;

static Symbol syms[32767];
static size_t nr_sym;
static char *name_unk = "??";

static int cmp(const void *a, const void *b) {
  uintN_t addra = ((Symbol *)a)->addr;
  uintN_t addrb = ((Symbol *)b)->addr;
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
      // goto section: sh[sh_link] (strtab)
      // read
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
        if (ELF_ST_TYPE(sym.st_info) == STT_FUNC ||
            ELF_ST_TYPE(sym.st_info) == STT_OBJECT) {
          if (nr_sym >= ARRLEN(syms)) {
            nr_sym++;
            continue;
          }
          syms[nr_sym++] = (Symbol) {
            .addr = sym.st_value,
            .size = sym.st_size,
            .name = savestring(&strtab[sym.st_name]),
          };
        }
      }
      free(strtab);
      break;
    }
  }
  size_t nr_sym_read = nr_sym < ARRLEN(syms) ? nr_sym : ARRLEN(syms);
  printf("Read %zu/%zu symbols from %s\n", nr_sym_read, nr_sym, filename);
  nr_sym = nr_sym_read;
  qsort(syms, nr_sym, sizeof(Symbol), cmp);
  for (int i = 0; i < nr_sym; i++)
    printf("name='%s', %#lx, %ld\n", syms[i].name, (long)syms[i].addr, (long)syms[i].size);
  fclose(f);
}

void elf_getname_and_offset(uintN_t addr, char **name, uintN_t *offset) {
  uintN_t uhole;
  char *nhole;
  if (name == NULL) name = &nhole;
  if (offset == NULL) offset = &uhole;
  for (int i = 0; i < nr_sym; i++) {
    if (syms[i].addr <= addr && addr < syms[i].addr + syms[i].size) {
      *name = syms[i].name;
      *offset = addr - syms[i].addr;
      return;
    }
  }
  *name = name_unk;
  *offset = -1;
}
