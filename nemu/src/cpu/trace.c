/* Everything about tracer
 * There are 5 independent tracer:
 * 1. instruction tracer
 * 2. instruction ring tracer
 * 3. memory tracer
 * 4. function tracer
 * 5. device tracer (TBD)
 * One can enable or disable them in menuconfig.
 */
#include <common.h>
#include "trace.h"
#ifdef CONFIG_LIBDISASM
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

#ifdef CONFIG_TRACE
static bool g_itrace_stdout = 0;
void trace_init() {
  // TODO
}
void trace_set_itrace_stdout(int enable) {
  g_itrace_stdout = enable;
}
 void do_itrace(Decode *s) {
#ifdef CONFIG_ITRACE
  if (g_itrace_stdout || ITRACE_COND) {
    char *buf = trace_disassemble(s);
    if (g_itrace_stdout)
      puts(buf);
    free(buf);
  }
#endif
}
#endif
