#include <fixedptc.h>
#include <stdio.h>

int main() {
  fixedpt a = FIXEDPT_PI / 6;
  fixedpt b = fixedpt_sin(a);
  printf("%x\n", b);
  return 0;
}
