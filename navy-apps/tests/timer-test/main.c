#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <NDL.h>

int main() {
  NDL_Init(0);
  const int period = 500;
  uint32_t timeus = NDL_GetTicks();
  int cnt = 0;
  while (1) {
    uint32_t timeus_cur = NDL_GetTicks();
    if ((timeus_cur - timeus) / period > cnt) {
      printf("Hello World!\n");
      cnt = (timeus_cur - timeus) / period;
    }
  }
  NDL_Quit();
  return 0;
}
