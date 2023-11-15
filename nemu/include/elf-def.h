#include <inttypes.h>
#ifndef _ELF_DEF_H_
// elf
// elf format, by default, is hard coded to the same as ISA
#ifdef CONFIG_ISA64
#define ELF64
#endif

#ifdef ELF64
typedef uint64_t uintN_t;
#else
typedef uint32_t uintN_t;
#endif
void elf_getname_and_offset(uintN_t addr, char **name, uintN_t *offset);
#endif
#endif
