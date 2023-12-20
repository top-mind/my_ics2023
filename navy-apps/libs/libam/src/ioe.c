#include <am.h>
#include <assert.h>

bool ioe_init() {
  return true;
}

void ioe_read (int reg, void *buf) {
  printf("reg = %d\n", reg);
  switch (reg) {
    case AM_TIMER_CONFIG:
      ((AM_TIMER_CONFIG_T *)buf)->present = 1;
      break;
    defalut:
      fprintf(stderr, "Unknown ioe port %d\n", reg);
      assert(0);
  }
}
void ioe_write(int reg, void *buf) {
  assert(0);
}
