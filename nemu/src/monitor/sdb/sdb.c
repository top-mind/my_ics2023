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
#include <memory/paddr.h>
#include <memory/host.h>
#include <cpu/difftest.h>
#include "sdb.h"
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>
#include <elf-def.h>

#define NOMORE(args) (args == NULL || '\0' == args[strspn(args, " ")])

static int is_batch_mode = false;
void init_regex();
void init_wp_pool();
void init_sigint();
void trace_init();
#ifdef CONFIG_FTRACE
bool ftrace_enable_finish();
bool ftrace_disable_finish();
void ftrace_set_stopat_push(bool);
#endif

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char *rl_gets() {
  char *line_read = readline("(nemu) ");
  if (line_read == NULL) return NULL;
  int is_spaces = 1;
  for (char *p = line_read; *p; p++) {
    if (*p != ' ') {
      is_spaces = 0;
      break;
    }
  }
  if (is_spaces) {
    free(line_read);
    return strdup(history_get(history_length)->line);
  } else {
    add_history(line_read);
  }
  return line_read;
}

static FILE *fp_scripts[1000];
static char *filename_scripts[1000];
static int lineno_scripts[1000];
static int nr_fp;

static char *(*getcmd)() = rl_gets;
static char *file_gets();

static int cmd_Raise(char *args) {
  if (NOMORE(args)) {
    puts("Usage: Raise SIGNAL");
    return 0;
  }
  raise(atoi(args));
  return 0;
}
static int cmd_c(char *args) {
  cpu_exec(1);
  if (nemu_state.state == NEMU_STOP) {
    enable_breakpoints();
    cpu_exec(-1);
    disable_breakpoints();
  }
  return 0;
}
static int cmd_q(char *args) {
  free_all_breakpoints();
  return -1;
}
static int cmd_help(char *);
static int cmd_si(char *);
static int cmd_info(char *);
static int cmd_x(char *);
static int cmd_bt(char *);
static int cmd_p(char *);
static int cmd_finish(char *);
static int cmd_nf(char *);
static int cmd_save(char *);
static int cmd_load(char *);
static int cmd_b(char *args) {
  if (NOMORE(args)) {
    puts("Not impl. To developer: read gdb manual");
    return 0;
  }
  create_breakpoint(args);
  return 0;
}
static int cmd_w(char *args) {
  if (NOMORE(args)) {
    puts("Usage: w EXPR");
    return 0;
  }
  int n = create_watchpoint(args);
  if (n > 0) printf("Watchpoint %d: %s\n", n, args);
  return 0;
}
static int cmd_d(char *args) {
  if (NOMORE(args)) {
    char *str = readline("Delete all watchpoints? (y|N) ");
    if (str[0] == 'y') free_all_breakpoints();
    free(str);
    return 0;
  }
  int n = atoi(args);
  if (!delete_breakpoint(n)) { printf("No breakpoint %d.\n", n); }
  return 0;
}
static int cmd_attach(char *args) {
  if (NOMORE(args)) {
    difftest_attach();
  } else {
    printf("Usage: attach\n");
  }
  return 0;
}
static int cmd_detach(char *args) {
  if (NOMORE(args)) {
    difftest_detach();
  } else {
    printf("Usage: detach\n");
  }
  return 0;
}
static int cmd_elfadd(char *args) {
  if (NOMORE(args)) {
    printf("Usage: elf add FILE [range]\n");
    return 0;
  }
  char *filename = strtok(NULL, " ");
  uint64_t res[3] = {0, -1, 0};
  for (int i = 0; i < 3; i++) {
    char *pnum = strtok(NULL, " ");
    if (pnum == NULL) break;
    char *endptr;
    res[i] = strtoull(pnum, &endptr, 0);
    if (*endptr != '\0') {
      printf("Syntax error near '%s'\n", pnum);
      return 0;
    }
  }
  elf_add(filename, res[0], res[1], res[2]);
  return 0;
}
static int cmd_elfclean(char *args) {
  elf_clean();
  return 0;
}
static int cmd_elf(char *args) {
  if (NOMORE(args)) {
    printf("elf a\nelf d\n");
    return 0;
  }
  char *args_end = args + strlen(args);
  char *subcmd = strtok(args, " ");
  char *arg = subcmd + strlen(subcmd) + 1;
  if (arg >= args_end) arg = NULL;
  if (strcmp(subcmd, "a") == 0) {
    cmd_elfadd(arg);
  } else if (strcmp(subcmd, "d") == 0) {
    cmd_elfclean(arg);
  } else {
    printf("Unknown subcommand %s, try `elf'.\n", subcmd);
  }
  return 0;
}
static int cmd_source(char *args) {
  if (NOMORE(args)) {
    printf("Usage: source FILE\n");
    return 0;
  }
  if (nr_fp >= ARRLEN(fp_scripts)) {
    printf("Too many files opened\n");
    return 0;
  }
  FILE *fp = fopen(args, "r");
  if (fp == NULL) {
    printf("Failed to open file `%s': %s\n", args, strerror(errno));
    return 0;
  }
  filename_scripts[nr_fp] = strdup(args);
  lineno_scripts[nr_fp] = 0;
  fp_scripts[nr_fp++] = fp;
  getcmd = file_gets;
  return 0;
}

/* structure for a command
 * name: the command name. A successful match happens when user types a string being a prefix of
 * this name.
 * description: the description of this command. The 'help' command will display this field.
 * int handler(args): the function to handle this command.
 *   args: the arguments passed to this command.
 *   return: a negative value means the main loop should exit.
 *           other values will be ignored.
 */
static struct {
  const char *name;
  const char *description;
  int (*handler)(char *);
} cmd_table[] = {
  {"help", "Display informations about all supported commands", cmd_help},
  {"Raise", "Raise signal", cmd_Raise},
  {"c", "Continue the execution of the program", cmd_c},
  {"q", "Exit NEMU", cmd_q},
  {"si",
   "Step one instruction exactly.\nUsage: si [N]\n"
   "Argument N means step N times (or till program stops for another reason).",
   cmd_si},
  {"info", NULL, cmd_info},
  {"i",
   "Generic command for showing program and debugger states\n"
   "info r -- List of registers and their contents.\n"
   "info w -- Status all watchpoints\n"
   "info b -- Status all breakpoints\n"
   "info h -- History intructions",
   cmd_info},
  {"x",
   "Examine memory: x N EXPR\nEXPR is an expression for the memory address to examine.\n"
   "N is a repeat count. The specified number of 4 bytes are printed in hexadecimal. If negative "
   "number is specified, memory is examined backward from the address.",
   cmd_x},
  {"print", NULL, cmd_p},
  {"inspect", NULL, cmd_p},
  {"p", "Print value of expression EXPR.\nUsage: p EXPR", cmd_p},
  {"b", "Set breakpoint", cmd_b},
  {"w",
   "Set a watchpoint for EXPR.\nUsage: w EXPR\n"
   "A watchpoint stops execution of your program whenever the value of an expression changes. This "
   "feature is disabled if build without WATCHPOINT_STOP",
   cmd_w},
  {"d",
   "Delete all or some watchpoints.\nUsage: delete [CKECKPOINTNUM]...\n"
   "Arguments are watchpoint numbers with spaces in between.\n"
   "To delete all watchpoints, give no argument.",
   cmd_d},
  {"backtrace", NULL, cmd_bt},
  {"where", NULL, cmd_bt},
  {"bt", "Print backtrace of all stack frames", cmd_bt},
  {"finish", "Finish current function", cmd_finish},
  {"nf", "Execute untill entering a function", cmd_nf},
  {"attach", "Enter difftest mode", cmd_attach},
  {"detach", "Exit difftest mode", cmd_detach},
  {"save", "Save the current state", cmd_save},
  {"load", "Load the current state", cmd_load},
  {"elf",
   "Manage elf files\n"
   "elf a FILE [BEGIN [END [FUNCONLY]]]\n"
   "Add symbols in FILE to symbol table. If BEGIN and END are given, only symbols in the range "
   "[BEGIN, END) are added. FUNCONLY is used to indicate whether to add object symbols or only "
   "functions.\n"
   "elf d -- Clean all symbols(besides those in default file)",
   cmd_elf},
  {"source",
   "Read commands from file.\n"
   /*TODO*/
   "Note that the file '.sdbinit' is read automatically in this way when sdb is started",
   cmd_source},
};

#define NR_CMD ARRLEN(cmd_table)

int match_command(const char *cmd);

static int cmd_help(char *args) {
  /* extract the first argument */
  if (NOMORE(args)) {
    // print help info for all commands
    for (int i = 0; i < NR_CMD; i++) {
      if (cmd_table[i].description == NULL) {
        printf("%s, ", cmd_table[i].name);
        continue;
      }
      int nr_desc = 0;
      const char *strdesc = cmd_table[i].description;
      while (strdesc[nr_desc] && strdesc[nr_desc] != '\n') nr_desc++;
      printf("%s\t-\t%.*s\n", cmd_table[i].name, nr_desc, strdesc);
    }
  } else {
    char *arg = strtok(NULL, " ");
    int found = match_command(arg);
    if (found == NR_CMD)
      return 1;
    if (cmd_table[found].description == NULL ||
        (found > 0 && cmd_table[found - 1].description == NULL)) {
      while (found > 0 && cmd_table[found - 1].description == NULL) found--;
      while (cmd_table[found].description == NULL) {
        printf("%s, ", cmd_table[found].name);
        found++;
      }
      printf("%s\n", cmd_table[found].name);
    }
    printf("%s\n", cmd_table[found].description);
  }

  return 0;
}

static int cmd_si(char *args) {
  uint64_t n = 1;
  char *endptr = NULL;
  char *arg_n = strtok(NULL, " ");
  char *arg_more = strtok(NULL, "");
  if (!NOMORE(arg_more)) {
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
  if (n > 0) {
    cpu_exec(1);
    if (nemu_state.state == NEMU_STOP) {
      enable_breakpoints();
      cpu_exec(n - 1);
      disable_breakpoints();
    }
  }
  return 0;
}

static int cmd_info(char *args) {
  char *arg = strtok(NULL, " ");
  if (arg == NULL) {
    puts("info r  \tdisplay registers");
    puts("info b  \tstatus of breakpoints");
    puts("info w  \tstatus of watchpoints");
    puts("info h  \thistrory intructions");
  } else {
    if (strcmp(arg, "r") == 0) {
      isa_reg_display();
      // #endif
    } else if (strcmp(arg, "w") == 0) {
      print_watchpoints();
    } else if (strcmp(arg, "h") == 0) {
      MUXDEF(CONFIG_IQUEUE, void trace_display();
             trace_display(), puts("Please enable iring tracer"));
    } else if (strcmp(arg, "b") == 0) {
      print_all_breakpoints();
    } else {
      printf("Unknown subcommand %s, try `help info'.\n", arg);
    }
  }

  return 0;
}

static int cmd_p(char *args) {
  if (NOMORE(args)) {
    puts("Usage: p EXPR");
    return 0;
  }
  // trick to print pc in hex
  if (strcmp(args, "$pc") == 0) {
    printf(FMT_PADDR, cpu.pc);
    char *f_name;
    Elf_Off f_off;
    elf_getname_and_offset(cpu.pc, &f_name, &f_off);
    if (ELF_OFFSET_VALID(f_off))
      printf("[%s+%" MUXDEF(ELF64, PRIu64, PRIu32) "]", f_name, f_off);
    else
      printf("[%s]", f_name);
    printf("\n");
  } else {
    eval_t result = expr(args);
    if (result.type == EV_SUC) {
      peval(result);
      printf(" ");
      printf("%" MUXDEF(CONFIG_ISA64, PRIu64, PRIu32), result.value);
      char *f_name;
      Elf_Off f_off;
      elf_getname_and_offset(result.value, &f_name, &f_off);
      if (ELF_OFFSET_VALID(f_off)) printf("[%s+%" MUXDEF(ELF64, PRIu64, PRIu32) "]", f_name, f_off);
      printf("\n");
    } else {
      peval(result);
      printf("\n");
    }
  }
  return 0;
}

static int cmd_x(char *args) {
  char *arg1 = strtok(NULL, " ");
  char *arg2 = strtok(NULL, ""); // extract verbatim
  if (NOMORE(arg2)) {
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
  if (errno == ERANGE) { puts("Numeric constant too large."); }

  eval_t res = expr(endptr + 1);
  if (res.type != EV_SUC) {
    peval(res);
    puts("");
    return 0;
  }
  word_t addr_begin = res.value;
  word_t addr_end = addr_begin + ((word_t)bytes << 2);

  if (addr_end < addr_begin) {
    word_t t = addr_end;
    addr_end = addr_begin;
    addr_begin = t;
  }

  word_t addr;
  for (addr = addr_begin; addr < addr_end; addr += sizeof(word_t)) {
    if (0 == (15 & (addr - addr_begin))) {
      if (addr != addr_begin) putchar('\n');
      printf(FMT_WORD ":", addr);
    }
    if (unlikely(!in_pmem(addr + sizeof(word_t) - 1) || !in_pmem(addr))) {
      printf("\tInvalid virtual address " FMT_PADDR, addr);
      break;
    }
    printf("\t" FMT_WORD, paddr_read(addr, sizeof(word_t)));
  }
  putchar('\n');
  return 0;
}

static int cmd_bt(char *args) {
  MUXDEF(CONFIG_FTRACE, void backtrace(); backtrace(), puts("Please enable ftrace"));
  return 0;
}

static int cmd_finish(char *args) {
  if (!NOMORE(args)) {
    printf("Usage: finish\n");
    return 0;
  }
#ifdef CONFIG_FTRACE
  if (!ftrace_enable_finish())
    printf("No function to finish\n");
  else
    cpu_exec(-1);
  ftrace_disable_finish();
#else
  printf("Please enable ftrace\n");
#endif
  return 0;
}

static int cmd_nf(char *args) {
  if (!NOMORE(args)) {
    printf("Usage: nf\n");
    return 0;
  }
#ifdef CONFIG_FTRACE
  ftrace_set_stopat_push(1);
  ftrace_enable_finish();
  cpu_exec(-1);
  ftrace_disable_finish();
  ftrace_set_stopat_push(0);
#else
  printf("Please enable ftrace\n");
#endif
  return 0;
}

static int cmd_save(char *args) {
  if (NOMORE(args)) {
    printf("Usage: save FILE\n");
    return 0;
  }
  FILE *fp = fopen(args, "w");
  if (fp == NULL) {
    printf("Failed to open file %s: %s\n", args, strerror(errno));
    return 0;
  }
  fprintf(fp, "%s\ntime = %" PRIi64 "\n", str(__GUEST_ISA__), sdb_get_start_time() - sdb_realtime());
  fflush(fp);
  int fd = fileno(fp);
  int dup_stdout = dup(STDOUT_FILENO);
  fflush(stdout);
  dup2(fd, STDOUT_FILENO);
  isa_reg_display();
  fflush(stdout);
  dup2(dup_stdout, STDOUT_FILENO);
  close(dup_stdout);
  fprintf(fp, "# if program don't work properly, try set time = %" PRIi64 "\n", sdb_realtime());
  fclose(fp);
  char *pathzlib = malloc(strlen(args) + 5);
  strcpy(pathzlib, args);
  strcat(pathzlib, ".zlib");
  fp = fopen(pathzlib, "w");
  free(pathzlib);
  if (fp == NULL) {
    printf("Failed to open file %s: %s\n", pathzlib, strerror(errno));
    return 0;
  }
  uLongf dst_len = compressBound(CONFIG_MSIZE);
  void *dst = malloc(dst_len);
  assert(dst != NULL);
  if (compress2(dst, &dst_len, guest_to_host(CONFIG_MBASE), CONFIG_MSIZE, Z_BEST_COMPRESSION)) {
    printf("save: failed to compress memory\n");
    return 0;
  }
  fwrite(dst, 1, dst_len, fp);
  free(dst);
  fclose(fp);
  return 0;
}

static int cmd_load(char *args) {
  FILE *fp;
  char isa[16];
  CPU_state _state;
  uint64_t time;
  // sanity check
  if (NOMORE(args)) {
    printf("Usage: load FILE\n");
    return 0;
  }
  fp = fopen(args, "r");
  if (fp == NULL) {
    printf("Failed to open file %s: %s\n", args, strerror(errno));
    return 0;
  }
  if (NULL == fgets(isa, sizeof(isa), fp)) {
    fclose(fp);
    printf("Bad file format.\n");
    return 0;
  }
  if (strcmp(isa, str(__GUEST_ISA__) "\n") != 0) {
    fclose(fp);
    printf("ISA mismatch.");
    return 0;
  }
  if (fscanf(fp, "time = %" PRIu64 "\n", &time) != 1) {
    fclose(fp);
    printf("Bad file format.\n");
    return 0;
  }
  // detach first
  difftest_detach();
  if (!isa_reg_load(fp, &_state)) {
    fclose(fp);
    printf("Can not load %s\n", args);
    return 0;
  }
  fclose(fp);
  
  // load memory
  char *pathzlib;
  void *src;
  uLongf src_len;
  void *dst = guest_to_host(CONFIG_MBASE);
  uLongf dst_len = CONFIG_MSIZE;

  pathzlib = malloc(strlen(args) + 5);
  strcpy(pathzlib, args);
  strcat(pathzlib, ".zlib");
  fp = fopen(pathzlib, "r");
  free(pathzlib);
  if (fp == NULL) {
    printf("Failed to open file %s: %s\n", pathzlib, strerror(errno));
    return 0;
  }
  fseek(fp, 0, SEEK_END);
  src_len = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  src = malloc(src_len);
  assert(fread(src, 1, src_len, fp) == src_len);
  fclose(fp);
  printf("Decompressing dump file %s.zlib...\n", args);
  if (uncompress(dst, &dst_len, src, src_len)) {
    free(src);
    printf("load: failed to uncompress memory\n");
    nemu_state.state = NEMU_END;
    return 0;
  }
  free(src);
  memcpy(&cpu, &_state, sizeof(cpu));
  if (time & 0x8000000000000000ull)
    sdb_set_start_time(sdb_realtime() + time);
  else
    sdb_set_start_time(time);
  nemu_state.state = NEMU_STOP;
  return 0;
}

void sdb_set_batch_mode() { is_batch_mode = true; }

int match_command(const char *cmd) {
  int found = NR_CMD;
  int cmdlen = strlen(cmd);
  int ambiguous = 0;
  for (int i = 0; i < NR_CMD; i++) {
    if (strncmp(cmd, cmd_table[i].name, cmdlen) == 0) {
      if (cmd_table[i].name[cmdlen] == '\0')
        return i;
      if (found == NR_CMD) {
        found = i;
      } else {
        ambiguous = 1;
      }
    }
  }
  if (found == NR_CMD || ambiguous) {
    if (ambiguous) {
      printf("Ambiguous command '%s': %s", cmd, cmd_table[found].name);
      for (int i = found + 1; i < NR_CMD; i++) {
        if (strncmp(cmd, cmd_table[i].name, cmdlen) == 0) { printf(", %s", cmd_table[i].name); }
      }
      printf(".\n");
    } else if (found == NR_CMD) {
      printf("Unknown command '%s'\n", cmd);
    }
  }
  return ambiguous ? NR_CMD : found;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; ((str = getcmd()) != NULL);) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    int ret = 0;
    if (cmd == NULL) goto finally;

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) { args = NULL; }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int found = match_command(cmd);
    if (found < NR_CMD)
      ret = cmd_table[found].handler(args);
    if (found == NR_CMD || ret > 0) {
      // error occurs in script
      if (getcmd != rl_gets) {
        for (int i = 0; i < nr_fp; i++) {
          printf("in %s:%d\n", filename_scripts[i], lineno_scripts[i]);
          free(filename_scripts[i]);
          fclose(fp_scripts[i]);
        }
        printf("Script exited with error\n");
        nr_fp = 0;
        getcmd = rl_gets;
      }
    }
  finally:
    free(str);
    if (ret < 0) break;
  }
}

static char *file_gets() {
  assert(fp_scripts[nr_fp - 1]);
  char *line_read = NULL;
  size_t n = 0;
  int ret;
  if ((ret = getline(&line_read, &n, fp_scripts[nr_fp - 1])) == -1) {
    free(line_read);
    fclose(fp_scripts[--nr_fp]);
    free(filename_scripts[nr_fp]);
    if (nr_fp == 0) {
      getcmd = rl_gets;
    }
    return getcmd();
  }
  lineno_scripts[nr_fp - 1]++;
  if (line_read[ret - 1] == '\n') line_read[ret - 1] = '\0';
  return line_read;
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();
  init_breakpoints();
  init_sigint();
  MUXDEF(CONFIG_TRACE, trace_init(), );
}
