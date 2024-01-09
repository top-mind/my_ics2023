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
#include <limits.h>
#include <memory/paddr.h>
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

static bool g_itrace_stdout = 0;

void trace_set_itrace_stdout(bool enable) { g_itrace_stdout = enable; }
void do_itrace(Decode *s) {
#ifdef CONFIG_ITRACE
  if (g_itrace_stdout || ITRACE_COND) {
    char *buf = trace_disassemble(s);
    if (g_itrace_stdout) puts(buf);
    if (ITRACE_COND) log_write("%s\n", buf);
    free(buf);
  }
#elif defined(CONFIG_LIBDISASM)
  if (g_itrace_stdout)
    printf("%s\n", trace_disassemble(s));
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
  iqueue_end++;
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
paddr_t stk_func[8192];

#ifdef CONFIG_FTRACE_COND
static inline void ftrace_push_printfunc(vaddr_t pc, int depth) {
  char *f_name;
  Elf_Off f_off;
  printf("%*.s", depth * 2, "");
  elf_getname_and_offset(pc, &f_name, &f_off);
  printf("%s", f_name);
  if (f_off != 0 && ELF_OFFSET_VALID(f_off)) printf("+0x%lx", (unsigned long)f_off);
  if (!ELF_OFFSET_VALID(f_off)) printf("[" FMT_PADDR "]", pc);
  printf("()");
}
static bool ras_tailcall = false;
static int ras_nr_repeat = 0;
static paddr_t ras_lastpc = 0;


void ftrace_flush() {
  if (ras_nr_repeat > 0) {
    if (ras_nr_repeat > FTRACE_COMPRESS_THRESHOLD)
      printf("; /* repeated %d times */\n", ras_nr_repeat);
    else {
      printf(";\n");
      for (int i = 1; i < ras_nr_repeat; i++) {
        ftrace_push_printfunc(ras_lastpc, ras_depth);
        printf(";\n");
      }
    }
    ras_nr_repeat = 0;
  }
}
#endif

static bool ftrace_stopat_push = false;

void ftrace_set_stopat_push(bool enable) { ftrace_stopat_push = enable; }

struct {
  vaddr_t addr;
  int totaltime;
  int cnt;
  int max5interval[5];
  int min5interval[5];
  int starttime;
} sdlcallstates[8192];

int elf_getid(vaddr_t);

void trace_showSDLcallstate() {
  extern uint64_t g_nr_guest_inst;
  static uint64_t last_nrinst = 0;
  if (last_nrinst == 0) {
    last_nrinst = g_nr_guest_inst;
    return;
  }
  last_nrinst = g_nr_guest_inst;
  printf("%"PRIu64"\n", (g_nr_guest_inst - last_nrinst));
  return;
  for (int i = 0; i < ARRLEN(sdlcallstates); i++) {
    if (sdlcallstates[i].cnt == 0) continue;
    char *name;
    elf_getname_and_offset(sdlcallstates[i].addr, &name, NULL);
    printf("%s: %d times, %d total, %d us, %d us, %d us, %d us, %d us\n", name, sdlcallstates[i].cnt,
           sdlcallstates[i].totaltime, sdlcallstates[i].max5interval[0],
           sdlcallstates[i].max5interval[1], sdlcallstates[i].max5interval[2],
           sdlcallstates[i].max5interval[3], sdlcallstates[i].max5interval[4]);
  }
}

uint64_t sdb_realtime();
void sdb_set_start_time(uint64_t);

void sdl_push(vaddr_t addr) {
  uint64_t time = get_time();
  char *name;
  elf_getname_and_offset(addr, &name, NULL);
  if (strncmp(name, "SDL_", 4) != 0) goto restore_time;
  int id = elf_getid(addr);
  assert(id >= 0);
  int interval = time - sdlcallstates[id].starttime;
  sdlcallstates[id].starttime = time;
  if (sdlcallstates[id].cnt == 0) {
    memset(&sdlcallstates[id].min5interval, 0x3f, sizeof(sdlcallstates[id].min5interval));
    sdlcallstates[id].addr = addr;
    goto restore_time;
  }
  int *pmax = &sdlcallstates[id].max5interval[0];
  int *pmin = &sdlcallstates[id].min5interval[0];
  for (int i = 0; i < ARRLEN(sdlcallstates[id].max5interval); i++) {
    if (interval > pmax[i]) {
      for (int j = ARRLEN(sdlcallstates[id].max5interval) - 1; j > i; j--) pmax[j] = pmax[j - 1];
      pmax[i] = interval;
      break;
    }
  }
  for (int i = 0; i < ARRLEN(sdlcallstates[id].min5interval); i++) {
    if (interval < pmin[i]) {
      for (int j = ARRLEN(sdlcallstates[id].min5interval) - 1; j > i; j--) pmin[j] = pmin[j - 1];
      pmin[i] = interval;
      break;
    }
  }
restore_time:
  sdb_set_start_time(sdb_realtime() - time);
}

void sdl_pop(vaddr_t addr) {
  uint64_t time = get_time();
  char *name;
  elf_getname_and_offset(addr, &name, NULL);
  if (strncmp(name, "SDL_", 4) != 0) goto restore_time;
  int id = elf_getid(addr);
  assert(id >= 0);
  if (sdlcallstates[id].starttime == 0) goto restore_time;
  sdlcallstates[id].cnt++;
  sdlcallstates[id].totaltime += time - sdlcallstates[id].starttime;
restore_time:
  sdb_set_start_time(sdb_realtime() - time);
}

/* Often, we print message if we prepare a whole line to print.  * But as soon
 * as program stop inside a function, sdb tells us we go into, as desired.  *
 * This may be modified when backtrace is available.  */
void ftrace_push(vaddr_t _pc, vaddr_t dnpc) {
  if (ras_depth < ARRLEN(stk_func)) stk_func[ras_depth] = _pc;
  sdl_push(dnpc);
  if (ftrace_stopat_push && nemu_state.state == NEMU_RUNNING) {
    char *f_name;
    elf_getname_and_offset(dnpc, &f_name, NULL);
    printf("Calling %s(" FMT_PADDR ")\n", f_name, dnpc);
    nemu_state.state = NEMU_STOP;
  }
#ifdef CONFIG_FTRACE_COND
  bool need_minus_nr_repeat, need_print_old, need_print_lbrace, need_print_new;
  need_print_old = ras_tailcall && ras_nr_repeat > 1;
  need_minus_nr_repeat = need_print_lbrace = ras_tailcall;
  need_print_new = ras_tailcall || dnpc != ras_lastpc || ras_nr_repeat == 0;
  // The last condition is necessary. Consider lastpc being initially a function entry.
  // That is, the first call is ((void (*)())0)()

  // flush
  if (need_minus_nr_repeat) ras_nr_repeat--;
  if (need_print_new) ftrace_flush();
  // print old
  if (need_print_old) ftrace_push_printfunc(ras_lastpc, ras_depth);
  if (need_print_lbrace) printf(" {\n");
  // print new
  if (need_print_new) ftrace_push_printfunc(dnpc, ras_depth);
  // update
  if (need_print_new)
    ras_lastpc = dnpc, ras_nr_repeat = 1;
  else
    ras_nr_repeat++;
  ras_tailcall = true;
#endif
  ras_depth++;
}

/* Two methods for sdb command finish
 * 1. sdb detect the end of a function by detecting the return instruction.
 * 2. sdb get an ISA dependent return address when entering a function.
 * The choice may depend on the ISA.
 * We use method 1. TODO method 2 and xbreak (gdb)
 */
size_t g_finish_depth; // if > 0, cmd finish is enabled
                       // stop when ras_depth == g_finish_depth
bool ftrace_enable_finish() {
  if (ras_depth == 0) return 0;
  g_finish_depth = ras_depth;
  return 1;
}
void ftrace_disable_finish() { g_finish_depth = 0; }

void ftrace_pop(vaddr_t pc, vaddr_t _dnpc) {
  if (ras_depth == 0) return;
  if (g_finish_depth == ras_depth) {
    char *f_name;
    elf_getname_and_offset(pc, &f_name, NULL);
    printf("finish from %s at " FMT_PADDR "\n", f_name, pc);
    nemu_state.state = NEMU_STOP;
  }
  --ras_depth;
#ifdef CONFIG_FTRACE_COND
  char *f_name;
  Elf_Off f_off;
  elf_getname_and_offset(pc, &f_name, &f_off);

  if (ras_tailcall) {
    ras_tailcall = false;
  } else {
    ftrace_flush();
    printf("%*.s", ras_depth * 2, "");
    printf("} /* %s */\n", f_name);
  }
#endif
}

static inline void print_frame(size_t id) {
  char *f_name;
  paddr_t addr = stk_func[id];
  elf_getname_and_offset(addr, &f_name, NULL);
  printf("# %zu " FMT_PADDR " in %s ()\n", ras_depth - id, addr, f_name);
}

void backtrace() {
  char *f_name;
  elf_getname_and_offset(cpu.pc, &f_name, NULL);
  printf("# 0 " FMT_PADDR " in %s ()\n", cpu.pc, f_name);
  size_t i = ras_depth - 1;
  if (ras_depth > ARRLEN(stk_func)) {
    printf("%u frame(s) folded\n", ras_depth - ARRLEN(stk_func));
    i = ARRLEN(stk_func) - 1;
  }
  for (; i > 0; i--) print_frame(i);
}

void trace_init() { ftrace_push(0, RESET_VECTOR); }

#endif
