#ifndef __TRACE_H__
#define __TRACE_H__
#include <common.h>
#ifdef CONFIG_TRACE
#include "cpu/decode.h"
static inline void do_iqtrace(Decode *s) {}
static inline void do_ftrace(Decode *s) { MUXDEF(CONFIG_FTRACE, isa_ras_update(s),); }
static bool g_print_step = false;

char *trace_disassemble(Decode *s);
static inline void do_itrace(Decode *s) {
  MUXDEF(
    CONFIG_ITRACE, if (CONFIG_ITRACE_COND || g_print_step) {
      char *assem = trace_disassemble(s);
      if (CONFIG_ITRACE_COND) log_write("%s\n", assem);
      if (g_print_step) puts(assem);
      free(assem);
    }, );
}


#endif
#endif
