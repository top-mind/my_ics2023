#ifndef __TRACE_H__
#define __TRACE_H__
#include <cpu/decode.h>

#define MAX_INST_TO_PRINT 10
static bool g_print_step = false;

#ifdef CONFIG_TRACE
char *trace_disassemble(Decode *s);
#ifdef CONFIG_IQUEUE
Decode iqueue_buf[CONFIG_NR_IQUEUE];
static size_t iqueue_end = 0;
static bool iquque_wrap = false;
#endif

static inline void do_ftrace(Decode *s) { MUXDEF(CONFIG_FTRACE, isa_ras_update(s), ); }
static inline void do_itrace(Decode *s) {
}

#endif
#endif
