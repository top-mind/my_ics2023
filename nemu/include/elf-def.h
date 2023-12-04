#ifndef _ELF_DEF_H_
#define _ELF_DEF_H_
#include <common.h>
#include <elf.h>
// elf format, by default, is hard coded to the same as ISA
#ifdef CONFIG_ISA64
#define ELF64
#endif

#define _DEF_ELF_TYPES(type, N) typedef Elf##N##_##type Elf_##type;

#ifdef ELF64
#define DEF_ELF_TYPES(type) _DEF_ELF_TYPES(type, 64)
#else
#define DEF_ELF_TYPES(type) _DEF_ELF_TYPES(type, 32)
#endif

#define ELF_TYPES(_) _(Ehdr) _(Shdr) _(Sym) _(Addr) _(Word) _(Off)
#define ELFCLASS     MUXDEF(ELF64, ELFCLASS64, ELFCLASS32)
#define ELF_ST_TYPE  ELF32_ST_TYPE

MAP(ELF_TYPES, DEF_ELF_TYPES)

#define ELF_OFFSET_VALID(x) (~x)

void elf_getname_and_offset(Elf_Addr addr, char **name, Elf_Word *offset);

typedef struct {
  Elf_Addr st_value; /* Symbol value */
  Elf_Word st_size;  /* Symbol size */
  bool type_func;    /* code data or object data */
  char *name;
} Symbol;

Elf_Addr elf_find_func_byname(char *name);
const Symbol *elf_find_symbol_byname(char *name);
#endif
