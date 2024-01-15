#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[], char *envp[]);
extern char **environ;
void __libc_init_array (void);
void call_main(uintptr_t *args) {
  int argc = (int)args[0];
  char **argv = (char **)args[1];
  environ = (char **)args[argc + 1];
  __libc_init_array();
  exit(main(argc, argv, environ));
  assert(0);
}
