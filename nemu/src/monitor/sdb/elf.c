#include <errno.h>
#include <stdio.h>
#include <common.h>
#include <stdlib.h>
#include "elf-def.h"

#define EM_AVAILABLE EM_RISCV

#define section_fseek(i) fseek(f, ehdr.e_shoff + sizeof(shdr) * (i), SEEK_SET)
#define R(x)             assert(1 == fread(&x, sizeof(x), 1, f))

static Symbol syms[32767];
static size_t nr_sym;
static char *name_unk = "??";

static int cmp(const void *a, const void *b) {
  Elf_Addr addra = ((Symbol *)a)->st_value;
  Elf_Addr addrb = ((Symbol *)b)->st_value;
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

  Elf_Word nr_sh = ehdr.e_shnum;
  Elf_Shdr shdr;
  section_fseek(0);
  R(shdr);
  if (shdr.sh_size > nr_sh) nr_sh = shdr.sh_size;

  for (Elf_Word sh_idx = 1; sh_idx < nr_sh; sh_idx++) {
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
      Elf_Off sh_off = shdr.sh_offset, sh_size = shdr.sh_size, str_size;
      section_fseek(shdr.sh_link);
      R(shdr);
      char *strtab = malloc(str_size = shdr.sh_size);
      fseek(f, shdr.sh_offset, SEEK_SET);
      Assert(1 == fread(strtab, str_size, 1, f), "Bad elf");
      Elf_Sym sym;
      for (Elf_Off _off = 0; _off < sh_size; _off += sizeof(sym)) {
        fseek(f, sh_off + _off, SEEK_SET);
        R(sym);
        if (ELF_ST_TYPE(sym.st_info) == STT_FUNC || ELF_ST_TYPE(sym.st_info) == STT_OBJECT) {
          if (nr_sym >= ARRLEN(syms)) {
            nr_sym++;
            continue;
          }
          syms[nr_sym++] = (Symbol){
            .st_value = sym.st_value,
            .st_size = sym.st_size,
            .type_func = ELF_ST_TYPE(sym.st_info) == STT_FUNC,
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
  // for (int i = 0; i < nr_sym; i++)
  //   printf("name='%s', %#lx, %ld\n", syms[i].name, (long)syms[i].st_value, (long)syms[i].st_value);
  fclose(f);
}

void elf_getname_and_offset(Elf_Addr addr, char **name, Elf_Off *offset) {
  Elf_Off uhole;
  char *nhole;
  if (name == NULL) name = &nhole;
  if (offset == NULL) offset = &uhole;
  for (int i = 0; i < nr_sym; i++) {
    if (syms[i].st_value <= addr && addr < syms[i].st_value + syms[i].st_size) {
      *name = syms[i].name;
      *offset = addr - syms[i].st_value;
      return;
    }
  }
  *name = name_unk;
  *offset = -1;
}

Elf_Addr elf_find_func_byname(char *name) {
  for (size_t i = 0; i < nr_sym; i++) {
    if (syms[i].type_func && 0 == strcmp(syms[i].name, name)) return syms[i].st_value;
  }
  return -1;
}

const Symbol *elf_find_symbol_byname(char *name) {
  for (size_t i = 0; i < nr_sym; i++) {
    if (0 == strcmp(syms[i].name, name)) return &syms[i];
  }
  return NULL;
}
