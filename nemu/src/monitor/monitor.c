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

#include <isa.h>
#include <memory/paddr.h>

void init_rand();
void init_log(const char *log_file);
void init_mem();
void init_difftest(char *ref_so_file, long img_size, int port);
void init_device();
void init_sdb();
void init_disasm(const char *triple);
void init_addelf(char *);

static void welcome() {
  Log("Trace: %s",
      MUXDEF(CONFIG_TRACE, ANSI_FMT("ON", ANSI_FG_GREEN), ANSI_FMT("OFF", ANSI_FG_RED)));
  IFDEF(CONFIG_TRACE, Log("If trace is enabled, a log file will be generated "
                          "to record the trace. This may lead to a large log file. "
                          "If it is not necessary, you can disable it in menuconfig"));
  Log("Build time: %s, %s", __TIME__, __DATE__);
  printf("Welcome to %s-NEMU!\n", ANSI_FMT(str(__GUEST_ISA__), ANSI_FG_YELLOW ANSI_BG_RED));
  printf("For help, type \"help\"\n");
}

#ifndef CONFIG_TARGET_AM
#include <getopt.h>

void sdb_set_batch_mode();

static char *log_file = NULL;
static char *diff_so_file = NULL;
static char *img_file = NULL;
static int difftest_port = 1234;

static long load_img() {
  if (img_file == NULL) {
    Log("No image is given. Use the default build-in image.");
    return 4096; // built-in image size
  }

  FILE *fp = fopen(img_file, "rb");
  Assert(fp, "Can not open '%s'", img_file);

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);

  Log("The image is %s, size = %ld", img_file, size);

  fseek(fp, 0, SEEK_SET);
  int ret = fread(guest_to_host(RESET_VECTOR), size, 1, fp);
  assert(ret == 1);

  fclose(fp);
  return size;
}

static int parse_args(int argc, char *argv[]) {
  const struct option table[] = {
    {"batch", no_argument, NULL, 'b'},      {"log", required_argument, NULL, 'l'},
    {"diff", required_argument, NULL, 'd'}, {"port", required_argument, NULL, 'p'},
    {"help", no_argument, NULL, 'h'},       {"elf", required_argument, NULL, 'e'},
    {0, 0, NULL, 0},
  };
  int o;
  /*
   * GETOPT(3)
   * translate zh_cn zh_CN
   *
   * int getopt(int, char *const[], char *optstring);
   *
   * extern char *optarg;
   * extern int optind, opterr, optopt;
   *
   * getopt() 处理命令行参数，- 开头，但不是 - 或 -- 的参数(element of
   * argv)是选项参数 (option element), 除了开头的 - 剩下的字符是选项(option
   * characters).  如果反复调用. 则依次返回每个选项.
   *
   * optind 变量指示下一个被处理的参数在 argv 中的下标, 初始为 1.
   *
   * 若有，getopt 返回下一个选项，更新 optind 和静态链接变量 nextchar。下一次调用
   * 可以恢复扫描后续的选项或argv参数。(译者注：当 optind 在两次调用之间被改变时，
   * nextchar 失去作用)
   *
   * 若无，返回 -1. optind 指示第一个非选项参数。
   *
   * optstring
   */
  while ((o = getopt_long(argc, argv, "-bhl:d:p:e:", table, NULL)) != -1) {
    switch (o) {
      case 'b': sdb_set_batch_mode(); break;
      case 'p': sscanf(optarg, "%d", &difftest_port); break;
      case 'l': log_file = optarg; break;
      case 'd': diff_so_file = optarg; break;
      case 'e': init_addelf(optarg); break;
      case 1:
                img_file = optarg;
                // if exists optarg:r.elf, then load it
                {
                  size_t len = strlen(optarg);
                  // test if optarg ends with ".bin"
                  if (len > 4 && strcmp(optarg + len - 4, ".bin") == 0) {
                    char *relf = savestring(optarg);
                    strcpy(relf+ len - 3, "elf");
                    init_addelf(relf);
                    free(relf);
                  }
                }
                return 0;
      default:
        printf("Usage: %s [OPTION...] IMAGE [args]\n\n", argv[0]);
        printf("\t-h,--help               show this help and exit\n");
        printf("\t-b,--batch              run with batch mode\n");
        printf("\t-l,--log=FILE           output log to FILE\n");
        printf("\t-d,--diff=REF_SO        run DiffTest with reference REF_SO\n");
        printf("\t-p,--port=PORT          run DiffTest with port PORT\n");
        printf("\t-e,--elf=FILE           get symbols from FILE(s) to resolve symbols\n");
        printf("\n");
        exit(0);
    }
  }
  return 0;
}

void init_monitor(int argc, char *argv[]) {
  /* Perform some global initialization. */

  /* Parse arguments. */
  parse_args(argc, argv);

  /* Set random seed. */
  init_rand();

  /* Open the log file. */
  init_log(log_file);

  /* Initialize memory. */
  init_mem();

  /* Initialize devices. */
  IFDEF(CONFIG_DEVICE, init_device());

  /* Perform ISA dependent initialization. */
  init_isa();

  /* Load the image to memory. This will overwrite the built-in image. */
  long img_size = load_img();

  /* Initialize differential testing. */
  init_difftest(diff_so_file, img_size, difftest_port);

  /* Initialize the simple debugger. */
  init_sdb();

#if defined(CONFIG_ITRACE) || defined(CONFIG_IQUEUE)
#ifndef CONFIG_ISA_loongarch32r
// #define INIT(name) init_disasm(#name "-pc-linux-gnu")
// #ifdef CONFIG_ISA_x86
//   INIT(i686);
// #elif defined(CONFIG_ISA_mips32)
//   INIT(mipsel);
// #elif defined(CONFIG_ISA_riscv)
// #ifdef CONFIG_RV64
//   INIT(riscv64);
// #else
//   INIT(riscv32);
// #endif /* CONFIG_RV64 */
// #endif /* isa-type */
  // clang-format off
  init_disasm(MUXDEF(CONFIG_ISA_x86, "i686",
              MUXDEF(CONFIG_ISA_mips32, "mipsel",
              MUXDEF(CONFIG_ISA_riscv, "",))) "-pc-linux-gnu");
  // clang-format on
#endif /* CONFIG_ISA_loongarch32r */
#endif /* ITRACE IQUEUE */

  /* Display welcome message. */
  welcome();
}
#else // CONFIG_TARGET_AM
static long load_img() {
  extern char bin_start, bin_end;
  size_t size = &bin_end - &bin_start;
  Log("img size = %ld", size);
  memcpy(guest_to_host(RESET_VECTOR), &bin_start, size);
  return size;
}

void am_init_monitor() {
  init_rand();
  init_mem();
  init_isa();
  load_img();
  IFDEF(CONFIG_DEVICE, init_device());
  welcome();
}
#endif
// vim: fenc=utf-8
