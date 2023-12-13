#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

int main() {
  // struct timeval tv;
  while (1) {
    // gettimeofday(&tv, NULL);
    usleep(500000);
  }
  return 0;
}
