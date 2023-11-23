#ifndef __TRACE_H__
#define __TRACE_H__
#include <common.h>
#include "cpu/decode.h"
static bool g_print_step = false;
#define MAX_INST_TO_PRINT 10
#ifdef CONFIG_TRACE
char *trace_disassemble(Decode *s);

static inline void do_iqtrace(Decode *s) {}
static inline void do_ftrace(Decode *s) { MUXDEF(CONFIG_FTRACE, isa_ras_update(s),); }
static inline void do_itrace(Decode *s) {
  MUXDEF(
    CONFIG_ITRACE, if (CONFIG_ITRACE_COND || g_print_step) {
      char *assem = trace_disassemble(s);
      if (CONFIG_ITRACE_COND) log_write("%s\n", assem);
      if (g_print_step) puts(assem);
      free(assem);
    }, );
}

char *trace_disassemble(Decode *s) {
  const int nrbuf = 128;
  char *p, *buf = p = malloc(nrbuf);
  p += snprintf(p, nrbuf, FMT_WORD ":", s->pc);
  int ilen = s->snpc - s->pc;
  int i;
  uint8_t *inst = (uint8_t *)&s->isa.inst.val;
  for (i = ilen - 1; i >= 0; i--) { p += snprintf(p, 4, " %02x", inst[i]); }
  int ilen_max = MUXDEF(CONFIG_ISA_x86, 8, 4);
  int space_len = ilen_max - ilen;
  if (space_len < 0) space_len = 0;
  space_len = space_len * 3 + 1;
  memset(p, ' ', space_len);
  p += space_len;

#ifndef CONFIG_ISA_loongarch32r
  void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  disassemble(p, buf + nrbuf - p, MUXDEF(CONFIG_ISA_x86, s->snpc, s->pc),
              (uint8_t *)&s->isa.inst.val, ilen);
#else
  p[0] = '\0'; // the upstream llvm does not support loongarch32r
#endif
  return buf;
}
#endif
#endif
