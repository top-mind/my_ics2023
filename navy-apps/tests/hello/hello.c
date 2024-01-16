#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>

void * b;

char *exe_argv[] = {NULL};

int main(int argc, char *argv[], char *envp[]) {
  printf("Hello World!\n");
  printf("argc = %d\n", argc);
  for (int i = 0; i < argc; i ++) {
    printf("argv[%d] = %s\n", i, argv[i]);
  }
  return 0;
  write(1, "Hello World!\n", 13);
  int i = 2;
  volatile int j = 0;
  while (1) {
    j ++;
    if (j == 10000) {
      printf("Hello World from Navy-apps for the %dth time!\n", i ++);
      j = 0;
    }
  }
  return 0;
}
