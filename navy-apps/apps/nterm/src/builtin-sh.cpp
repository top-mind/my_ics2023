#include <nterm.h>
#include <stdarg.h>
#include <unistd.h>
#include <SDL.h>
#include <string.h>
#include <errono.h>

char handle_key(SDL_Event *ev);

static void sh_printf(const char *format, ...) {
  static char buf[256] = {};
  va_list ap;
  va_start(ap, format);
  int len = vsnprintf(buf, 256, format, ap);
  va_end(ap);
  term->write(buf, len);
}

static void sh_banner() {
  sh_printf("Built-in Shell in NTerm (NJU Terminal)\n\n");
}

static void sh_prompt() {
  sh_printf("sh> ");
}

static void sh_handle_cmd(const char *cmd) {
  char * tmp = strdup(cmd);
  tmp = strtok(tmp, " \n");
  if (tmp == NULL) return;
  if (strcmp(tmp, "echo") == 0) {
    char *args = strtok(NULL, "");
    if (args != NULL) {
      args += strspn(args, " ");
      term->write(args, strlen(args));
    }
  } else if (strcmp(tmp, "exit") == 0) {
    exit(0);
  }
  setenv("PATH", "/bin:/usr/bin", 0);
  const int max_argc = 16;
  char *argv[max_argc];
  argv[0] = tmp;
  int i;
  for (i = 1; i < max_argc; i++) {
    argv[i] = strtok(NULL, " \n");
    if (argv[i] == NULL) break;
  }
  if (i == max_argc) {
    term->write("Too many arguments\n", 19);
    return;
  }
  execvp(tmp, argv);
  perror("execvp fail");
}

void builtin_sh_run() {
  sh_banner();
  sh_prompt();

  while (1) {
    SDL_Event ev;
    if (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_KEYUP || ev.type == SDL_KEYDOWN) {
        const char *res = term->keypress(handle_key(&ev));
        if (res) {
          sh_handle_cmd(res);
          sh_prompt();
        }
      }
    }
    refresh_terminal();
  }
}
