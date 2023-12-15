#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>

int main() {
  int fd = open("/dev/events", 0, 0);
  if (fd < 0)
    assert(0);
  lseek(fd, 0, SEEK_SET);
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
