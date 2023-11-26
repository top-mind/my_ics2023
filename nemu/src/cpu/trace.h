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

static inline void do_iqueue(Decode *s) {
#ifdef CONFIG_IQUEUE
  memcpy(iqueue_buf + iqueue_end++, s, sizeof(Decode));
  if (iqueue_end == CONFIG_NR_IQUEUE) {
    iqueue_end = 0;
    iquque_wrap = 1;
  }
  Log("1 %zd %d", iqueue_end, iquque_wrap);
#endif
}
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

void trace_show_message_error() {
#ifdef CONFIG_IQUEUE
#define disasm_print                                 \
  do {                                               \
    char *assem = trace_disassemble(iqueue_buf + i); \
    puts(assem);                                     \
    free(assem);                                     \
  } while (0)
  size_t i;
  if (iquque_wrap)
    for (i = iqueue_end; i < CONFIG_NR_IQUEUE; i++)
      disasm_print;
  for (i = 0; i < iqueue_end; i++)
    disasm_print;
#undef disasm_print
#endif
}

#endif
