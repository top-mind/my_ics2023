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

#include <errno.h>
#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "common.h"
#include "sdb.h"
#include <sys/types.h>
#include <utils.h>
#include "memory/paddr.h"
#include "memory/host.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char *rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) { add_history(line_read); }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_help(char *);

static int cmd_si(char *);

static int cmd_info(char *);

static int cmd_x(char *);

static int cmd_p(char *args) {
  bool suc;
  word_t result = expr(args, &suc);
  if (suc) printf("%" PRIu32 "\n", result);
  return 0;
}

static int cmd_w(char *args) { return 0; }

static int cmd_d(char *args) { panic("cmd_d"); }

static struct {
  const char *name;
  const char *description;
  int (*handler)(char *);
} cmd_table[] = {
  {"help", "Display informations about all supported commands", cmd_help},
  {"c", "Continue the execution of the program", cmd_c},
  {"q", "Exit NEMU", cmd_q},
  {"si",
   "Step one instruction exactly.\nUsage: si [N]\n"
   "Argument N means step N times (or till program stops for another reason).",
   cmd_si},
  {"info",
   "\tinfo r -- List of registers and their contents.\n"
   "\tinfo w -- Status all watchpoints",
   cmd_info},
  {"x",
   "Examine memory: x N EXPR\nEXPR is an expression for the memory address to examine.\n"
   "N is a repeat count. The specified number of 4 bytes are printed in hexadecimal. If negative "
   "number is specified, memory is examined backward from the address.",
   cmd_x},
  {"p", "Print value of expression EXPR.\nUsage: p EXPR", cmd_p},
  {"w",
   "Set a watchpoint for EXPR.\nUsage: watch EXPR\n"
   "A watchpoint stops execution of your program whenever the value of an expression changes. This "
   "feature is disabled if build without WATCHPOINT_STOP",
   cmd_w},
  {"d",
   "Delete all or some watchpoints.\nUsage: delete [CKECKPOINTNUM]...\n"
   "Arguments are watchpoint numbers with spaces in between.\n"
   "To delete all watchpoints, give no argument.",
   cmd_d},
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  } else {
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_si(char *args) {
  uint64_t n = 1;
  char *endptr = NULL;
  char *arg_n = strtok(NULL, " ");
  char *arg_more = strtok(NULL, "");
  if (arg_more != NULL && arg_more[strspn(arg_more, " ")]) {
    puts("Usage: si [N]");
    return 0;
  }
  if (arg_n != NULL) {
    n = strtoull(arg_n, &endptr, 0);
    // If no number parsed, default to 1
    if (*endptr != '\0') {
      printf("Invalid character '%c'.\n", *endptr);
      return 0;
    }
  }
  cpu_exec(n);
  return 0;
}

static int cmd_info(char *args) {
  char *arg = strtok(NULL, " ");
  if (arg == NULL) {
    puts("info r  \tdisplay registers");
    puts("info w  \tstatus watchpoints");
  } else {
    if (strcmp(arg, "r") == 0) {
      isa_reg_display();
#ifdef CONFIG_ITRACE
      printf("%-15s" FMT_WORD, "pc", cpu.pc);
      fflush(stdout); // llvm::outs() aka raw_string write doesn't sync stdout
      Decode s = {.pc = cpu.pc, .snpc = cpu.pc};
      isa_fetch_decode(&s);
      void disassemblePrint(uint64_t pc, uint8_t * code, int nbyte);
      disassemblePrint(MUXDEF(CONFIG_ISA_x86, s.snpc, s.pc), (uint8_t *)&s.isa.instr.val,
                       s.snpc - s.pc);
      putchar('\n');
#else
      printf("%-15s0x%-" MUXDEF(CONFIG_ISA64, "16l", "8") "x\n", "pc", cpu.pc);
#endif
    } else if (strcmp(arg, "w") == 0)
      panic("info w");
    else
      printf("Unknown symbol %s, try help info.\n", arg);
  }

  return 0;
}

static int cmd_x(char *args) {
  char *arg1 = strtok(NULL, " ");
  char *arg2 = strtok(NULL, ""); // extract verbatim
  if (arg2 == NULL || '\0' == arg2[strspn(arg2, " ")]) {
    puts("Argument(s) required");
    puts("Usage: x N EXPR");
    return 0;
  }
  char *endptr;
  long long bytes = strtoull(arg1, &endptr, 0);
  if (*endptr != '\0') {
    printf("Invalid character '%c', require octal/decimal/hexadecimal.\n", *endptr);
    return 0;
  }
  if (errno == ERANGE) {
    puts("Numeric constant too large.");
  }

  bool success;
  word_t addr_begin = expr(endptr + 1, &success);
  if (!success) return 0;
  word_t addr_end = addr_begin + ((word_t)bytes << 2);

  if (addr_end < addr_begin) {
    word_t t = addr_end;
    addr_end = addr_begin;
    addr_begin = t;
  }

  word_t addr;
  for (addr = addr_begin; addr < addr_end; addr += 4) {
    if (0 == (15 & (addr - addr_begin))) {
      if (addr != addr_begin) putchar('\n');
      printf(FMT_WORD ":", addr);
    }
    if (unlikely(!in_pmem(addr + 3) || !in_pmem(addr))) {
      printf("\tInvalid virtual address " FMT_PADDR, addr);
      break;
    }
    printf("\t0x%08" PRIx32, (uint32_t)paddr_read(addr, 4));
  }
  putchar('\n');
  return 0;
}

void sdb_set_batch_mode() { is_batch_mode = true; }

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL;) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) { args = NULL; }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
