#ifndef __TRACE_H__
#define __TRACE_H__
#include <cpu/decode.h>

#ifdef CONFIG_TRACE
#define MAX_INST_TO_PRINT         10
#define FTRACE_COMPRESS_THRESHOLD 1
void do_trace(Decode *);
void irtrace_print(uint64_t total);
void trace_set_itrace_stdout(bool enable);
#endif

#ifdef CONFIG_FTRACE_COND
void ftrace_flush();
#endif
#endif
