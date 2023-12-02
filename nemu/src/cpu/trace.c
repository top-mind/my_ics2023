/* Everything about tracer
 * There are 5 independent tracer:
 * 1. instruction tracer
 * 2. instruction ring tracer
 * 3. memory tracer
 * 4. function tracer
 * 5. device tracer (TBD)
 * One can enable or disable them in menuconfig.
 * This file do not contain impl of memory tracer and device tracer.
 * See paddr.c and device.c(TBD) for more details.S
 */
#include <common.h>
#include "trace.h"
#include <isa.h>
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

#ifdef CONFIG_IQUEUE
#endif
static bool g_itrace_stdout = 0;
void trace_init() {
  // TODO
}
void trace_set_itrace_stdout(bool enable) {
  g_itrace_stdout = enable;
}
void do_itrace(Decode *s) {
#ifdef CONFIG_ITRACE
  if (g_itrace_stdout || ITRACE_COND) {
    char *buf = trace_disassemble(s);
    if (g_itrace_stdout) puts(buf);
    if (ITRACE_COND) log_write("%s\n", buf);
    free(buf);
  }
#endif
}

#ifdef CONFIG_IQUEUE
static Decode iqueue[CONFIG_NR_IQUEUE];
static int iqueue_end = 0;
static bool iqueue_wrap = 0;
#endif

static inline void do_irtrace(Decode *s) {
#ifdef CONFIG_IQUEUE
  iqueue[iqueue_end] = *s; // It is better to use ISA depended method to copy ISADecodeInfo
  iqueue_end ++;
  if (iqueue_end == CONFIG_NR_IQUEUE) {
    iqueue_end = 0;
    iqueue_wrap = 1;
  }
#endif
}

static inline void do_ftrace(Decode *s) { MUXDEF(CONFIG_FTRACE, isa_ras_update(s), ); }

void do_trace(Decode *s) {
  do_itrace(s);
  do_irtrace(s);
  do_ftrace(s);
}

void irtrace_print(uint64_t total) {
#ifdef CONFIG_IQUEUE
  int i;
  printf("Last %d/%" PRIu64 " instructions:\n", iqueue_wrap ? CONFIG_NR_IQUEUE : iqueue_end, total);
  if (iqueue_wrap) {
    for (i = iqueue_end; i < CONFIG_NR_IQUEUE; i++) {
      char *buf = trace_disassemble(&iqueue[i]);
      puts(buf);
      free(buf);
    }
  }
  for (i = 0; i < iqueue_end; i++) {
    char *buf = trace_disassemble(&iqueue[i]);
    puts(buf);
    free(buf);
  }
#endif
}

#include <elf-def.h>
static int ras_depth = 0;
static bool ras_tailcall = false;
static paddr_t ras_lastpc = 0;
static int ras_nr_repeat = 0;

void ftrace_flush() {
  if (ras_nr_repeat > 0) {
    printf("; /* repeated %d times */\n", ras_nr_repeat);
    ras_nr_repeat = 0;
  }
  ras_lastpc = 0;
}

static inline void ftrace_push_printfunc(vaddr_t pc, int depth) {
  char *f_name;
  uintN_t f_off;
  printf("%*.s", depth * 2, "");
  elf_getname_and_offset(pc, &f_name, &f_off);
  printf("%s", f_name);
  if (f_off != 0 && ELF_OFFSET_VALID(f_off))
    printf("+0x%lx", (unsigned long) f_off);
  if (!ELF_OFFSET_VALID(f_off))
    printf("[" FMT_PADDR "]", pc);
  printf("()");
  fflush(stdout);
}

void ftrace_push(vaddr_t _pc, vaddr_t dnpc) {
  bool need_minus_nr_repeat, need_print_old, need_print_lbrace, need_print_new;
  need_print_old = ras_tailcall && ras_nr_repeat > 1;
  need_minus_nr_repeat = need_print_lbrace = ras_tailcall;
  need_print_new = ras_tailcall || dnpc != ras_lastpc || ras_nr_repeat == 0;
  // The last condition is necessary. Consider lastpc being initially a function entry.
  // That is, the first call is ((void (*)())0)()
  // flush
  if (need_minus_nr_repeat) ras_nr_repeat--;
  ftrace_flush();
  // print old
  if (need_print_old)
    ftrace_push_printfunc(ras_lastpc, ras_depth);
  if (need_print_lbrace)
    printf(" {\n");
  // print new
  if (need_print_new)
    ftrace_push_printfunc(dnpc, ras_depth++);
  // update
  if (need_print_new) ras_lastpc = dnpc, ras_nr_repeat = 1;
  else ras_nr_repeat++;
  ras_tailcall = true;
}

void ftrace_pop(vaddr_t pc, vaddr_t _dnpc) {
  if (ras_depth == 0) return;
  --ras_depth;
  char *f_name;
  uintN_t f_off;
  elf_getname_and_offset(pc, &f_name, &f_off);

  if (ras_tailcall) {
    ras_tailcall = false;
  } else {
    ftrace_flush();
    printf("%*.s", ras_depth * 2, "");
    printf("} /* %s */\n", f_name);
  }
}

#endif
