#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

int main() {
  const int period = 2000;
  uint32_t timeus;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  timeus = tv.tv_sec * 1000 + tv.tv_usec / 1000;
  int cnt = 0;
  while (1) {
    gettimeofday(&tv, NULL);
    uint32_t timeus_cur = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    if ((timeus_cur - timeus) / period > cnt) {
      printf("Hello World!\n");
      cnt = (timeus_cur - timeus) / period;
    }
  }
  return 0;
}
