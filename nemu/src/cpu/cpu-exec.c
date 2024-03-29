/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <cpu/difftest.h>
#include <locale.h>
#include "trace.h"
#include "utils.h"

#ifndef CONFIG_TARGET_AM
#include <stdatomic.h>
  extern atomic_uchar a_nemu_state; 
#define SIGINT_HANDLER_BEGIN \
  do { \
    atomic_store(&a_nemu_state, 1); \
  } while (0)
#define SIGINT_HANDLER_END \
  do { \
    atomic_store(&a_nemu_state, 0); \
  } while (0)
#else
#define SIGINT_HANDLER_BEGIN
#define SIGINT_HANDLER_END
#endif

CPU_state cpu = {};
uint64_t g_nr_guest_inst = 0;
static uint64_t g_timer = 0; // unit: us

void device_update();
static void trace_and_difftest(Decode *_this, vaddr_t dnpc) {
#ifdef CONFIG_TRACE
  do_trace(_this);
#endif
#ifndef CONFIG_TARGET_AM
  void watchpoints_notify();
  watchpoints_notify();
#endif
  IFDEF(CONFIG_DIFFTEST, difftest_step(_this->pc, dnpc));
}

static void exec_once(Decode *s, vaddr_t pc) {
  s->pc = pc;
  s->snpc = pc;
  isa_exec_once(s);
  cpu.pc = s->dnpc;
}

static void execute(uint64_t n) {
  Decode s;
  for (; n > 0; n--) {
    exec_once(&s, cpu.pc);
    g_nr_guest_inst++;
    trace_and_difftest(&s, cpu.pc);
    IFNDEF(CONFIG_TARGET_AM, if (atomic_load(&a_nemu_state) == 2) nemu_state.state = NEMU_STOP,
           puts("Ctrl-C"));
    if (nemu_state.state != NEMU_RUNNING) break;
    IFDEF(CONFIG_DEVICE, device_update());
    word_t intr = isa_query_intr();
    if (intr != INTR_EMPTY) {
      cpu.pc = isa_raise_intr(intr, cpu.pc);
    }
  }
}

static void statistic() {
  IFNDEF(CONFIG_TARGET_AM, setlocale(LC_NUMERIC, ""));
#define NUMBERIC_FMT MUXDEF(CONFIG_TARGET_AM, "%", "%'") PRIu64
  Log("host time spent = " NUMBERIC_FMT " us", g_timer);
  Log("total guest instructions = " NUMBERIC_FMT, g_nr_guest_inst);
  if (g_timer > 0)
    Log("simulation frequency = " NUMBERIC_FMT " inst/s", g_nr_guest_inst * 1000000 / g_timer);
  else
    Log("Finish running in less than 1 us and can not calculate the simulation frequency");
}

void trace_display() {
#ifdef CONFIG_IQUEUE
  irtrace_print(g_nr_guest_inst);
#endif
}

void print_fail_msg() {
  MUXDEF(CONFIG_FTRACE_COND, ftrace_flush(), );
  isa_reg_display();
  trace_display();
  statistic();
}

/* Simulate how the CPU works. */
void cpu_exec(uint64_t n) {
#ifdef CONFIG_TRACE
  trace_set_itrace_stdout(n < MAX_INST_TO_PRINT);
#endif
  switch (nemu_state.state) {
    case NEMU_END:
    case NEMU_ABORT:
      printf("Program execution has ended. To restart the program, exit NEMU and run again.\n");
      return;
    default: nemu_state.state = NEMU_RUNNING;
  }

  uint64_t timer_start = get_time();

  SIGINT_HANDLER_BEGIN;
  execute(n);
  SIGINT_HANDLER_END;

  MUXDEF(CONFIG_FTRACE_COND, ftrace_flush(), );

  uint64_t timer_end = get_time();
  g_timer += timer_end - timer_start;

  switch (nemu_state.state) {
    case NEMU_RUNNING: nemu_state.state = NEMU_STOP; break;
    case NEMU_INT:
      if (MUXDEF(CONFIG_TARGET_AM, 1, ({
                   bool breakpoint_notify(vaddr_t);
                   !breakpoint_notify(nemu_state.halt_pc);
                 }))) {
#ifdef CONFIG_ISA_riscv
        nemu_state.state = NEMU_END;
        goto nemu_end;
#elif defined(CONFIG_ISA_x86)
#endif
      }
      break;
    case NEMU_ABORT:
      switch (nemu_state.halt_ret) {
        case ABORT_INV:
          // err msg shown in isa_exec_once -> invalid_inst
          // nothing to do here
          break;
        case ABORT_MEMIO: print_fail_msg(); break;
      }
    nemu_end:
      Log("nemu: %s at pc = " FMT_WORD,
          (nemu_state.state == NEMU_ABORT
             ? ANSI_FMT("ABORT", ANSI_FG_RED)
             : (nemu_state.halt_ret == 0 ? ANSI_FMT("HIT GOOD TRAP", ANSI_FG_GREEN)
                                         : ANSI_FMT("HIT BAD TRAP", ANSI_FG_RED))),
          nemu_state.halt_pc);
      // fall through
    case NEMU_QUIT: statistic();
  }
}
