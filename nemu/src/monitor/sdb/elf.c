#include <elf.h>
#include <stdio.h>
#include <macro.h>
#include <common.h>

// elf format, by default, is hard coded to the same as ISA
#ifdef CONFIG_ISA64
#define ELF64
#endif

#define EM_AVAILABLE EM_RISCV

#define _DEF_ELF_TYPES(type, N) typedef Elf##N##_##type Elf_##type;

#ifdef ELF64
#define DEF_ELF_TYPES(type) _DEF_ELF_TYPES(type, 64)
typedef uint64_t uintN_t;
#else
#define DEF_ELF_TYPES(type) _DEF_ELF_TYPES(type, 32)
typedef uint32_t uintN_t;
#endif

#define ELF_TYPES(_) _(Ehdr) _(Shdr) _(Sym)
#define ELFCLASS MUXDEF(ELF64, ELFCLASS64, ELFCLASS32)
#define ELF_ST_TYPE ELF32_ST_TYPE

#define section_fseek(i) fseek(f, ehdr.e_shoff + sizeof(shdr) * (i), SEEK_SET)
#define R(x) assert(1 == fread(&x, sizeof(x), 1, f))

MAP(ELF_TYPES, DEF_ELF_TYPES)

void init_addelf(char *filename) {
  FILE *f = fopen(filename, "r");
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
      uintN_t sh_off = shdr.sh_offset,
              sh_size = shdr.sh_size,
              str_size;
      section_fseek(shdr.sh_link);
      char *strtab = malloc(str_size = shdr.sh_size);
      Elf_Sym sym;
      for (uintN_t _off = 0; _off < sh_size; _off += sizeof(sym)) {
        fseek(f, sh_off + _off, SEEK_SET);
        R(sym);
        if (ELF_ST_TYPE(sym.st_info) == STT_FUNC) {
          printf("name='%s', %lx, %lx\n", &strtab[sym.st_name],
                 (long)sym.st_value, (long)sym.st_size);
        }
      }
      free(strtab);
    }
  }
  fclose(f);
}
