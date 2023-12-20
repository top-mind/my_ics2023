#include <am.h>
#include "amdev.h"
#include <assert.h>
#include <NDL.h>

#define concat_temp(x, y) x ## y
#define concat(x, y) concat_temp(x, y)

int w, h;

bool ioe_init() {
  NDL_Init(0);
  NDL_OpenCanvas(&w, &h);
  return true;
}

#define IOE_QUICKSET(reg, buf, ...)                                            \
  case reg: {                                                                  \
    reg##_T _tmp = {__VA_ARGS__};                                              \
    *(reg##_T *)buf = _tmp;                                                    \
    break;                                                                     \
  }

void ioe_read (int reg, void *buf) {
  switch (reg) {
    IOE_QUICKSET(AM_TIMER_CONFIG, buf, .present = 1);
    IOE_QUICKSET(AM_GPU_CONFIG, buf, .present = 1, .height = h, .width = w);
    IOE_QUICKSET(AM_INPUT_CONFIG, buf, .present = 1);
    default:
      fprintf(stderr, "Unknown ioe read %d\n", reg);
      assert(0);
  }
}
void ioe_write(int reg, void *buf) {
  switch (reg) {
  case AM_GPU_FBDRAW: {
    AM_GPU_FBDRAW_T *tmp = (AM_GPU_FBDRAW_T *)buf;
    NDL_DrawRect(tmp->pixels, tmp->x, tmp->y, tmp->w, tmp->h);
    break;
  }
    default:
      fprintf(stderr, "Unknown ioe write %d\n", reg);
      assert(0);
  }
}
