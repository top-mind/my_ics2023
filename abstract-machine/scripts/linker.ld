ENTRY(_start)
PHDRS { text PT_LOAD; data PT_LOAD; }

SECTIONS {
  /* _pmem_start and _entry_offset are defined in LDFLAGS */
  . = _pmem_start + _entry_offset;
  .text : {
    *(entry)
    *(.text*)
  } : text
  etext = .;
  _etext = .;
  .rodata : {
    *(.rodata*)
    /* put in templete linker.script */
    /*
    . = ALIGN(0x4);
    _nemu_mainargs = .;
    INCLUDE "build/mainargs.ld"
    */
  }
  .data : {
    *(.data)
  } : data
  edata = .;
  _data = .;
  .bss : {
	_bss_start = .;
    *(.bss*)
    *(.sbss*)
    *(.scommon)
  }
  no_stack_top = ALIGN(0x1000);
  . = no_stack_top + 0x8000;
  _stack_pointer = .;
  end = .;
  _end = .;
   _heap_start = ALIGN(0x1000);
}
