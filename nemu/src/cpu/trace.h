#ifndef __TRACE_H__
#define __TRACE_H__
#include <common.h>
#ifdef CONFIG_TRACE
#include "cpu/decode.h"
static inline void do_iqtrace(Decode *s) {}
static inline void do_ftrace(Decode *s) { MUXDEF(CONFIG_FTRACE, isa_ras_update(s),); }
void do_itrace(Decode *);

char *trace_disassemble(Decode *s);
#endif
#endif
