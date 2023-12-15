#include <stdio.h>
#include <NDL.h>

int main() {
  NDL_Init(0);
  while (1) {
    char buf[10];
    int ret;
    if (0 != (ret =  NDL_PollEvent(buf, sizeof(buf)))) {
      printf("receive event: %s len = %d, %d\n", buf, strlen(buf), ret);
      assert(strlen(buf) == ret);
    }
  }
  return 0;
}
