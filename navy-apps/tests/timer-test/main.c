#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <NDL.h>

int main() {
  const int period = 2000;
  uint32_t timeus = NDL_GetTicks();
  int cnt = 0;
  while (1) {
    uint32_t timeus_cur = NDL_GetTicks();
    if ((timeus_cur - timeus) / period > cnt) {
      printf("Hello World!\n");
      cnt = (timeus_cur - timeus) / period;
    }
  }
  return 0;
}
