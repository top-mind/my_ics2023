#ifndef __TRACE_H__
#define __TRACE_H__
#include <cpu/decode.h>

#define MAX_INST_TO_PRINT 10

#ifdef CONFIG_TRACE
void trace_init();
void do_trace(Decode *);
void irtrace_print();
void trace_set_itrace_stdout(bool enable);
#endif

#endif
