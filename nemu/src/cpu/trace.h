#ifndef __TRACE_H__
#define __TRACE_H__
#include "cpu/decode.h"

#define MAX_INST_TO_PRINT 10
static bool g_print_step = false;

#ifdef CONFIG_TRACE
char *trace_disassemble(Decode *s);
// Decode iqueue_buf[];

static inline void do_iqtrace(Decode *s) {}
static inline void do_ftrace(Decode *s) { MUXDEF(CONFIG_FTRACE, isa_ras_update(s), ); }
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
