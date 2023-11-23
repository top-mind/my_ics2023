#ifndef __TRACE_H__
#include <cpu/decode.h>

static void do_itrace(Decode *);
static void do_iqtrace(Decode *s) {}
static void do_ftrace(Decode *s) { MUXDEF(CONFIG_FTRACE, isa_ras_update(s),); }

#endif
