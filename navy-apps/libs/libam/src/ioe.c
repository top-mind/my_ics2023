#include <am.h>
#include "amdev.h"
#include <assert.h>
#include <NDL.h>
#include <string.h>

#define concat_temp(x, y) x ## y
#define concat(x, y) concat_temp(x, y)
#define ARRLEN(x) (sizeof(x) / sizeof (x[0]))

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

#define KEY_NAMES(x) #x,

static const char *keynames[] = {
  "NONE",
  AM_KEYS(KEY_NAMES)
};

void ioe_read (int reg, void *buf) {
  switch (reg) {
    IOE_QUICKSET(AM_TIMER_CONFIG, buf, .present = 1);
    IOE_QUICKSET(AM_GPU_CONFIG, buf, .present = 1, .height = h, .width = w);
    IOE_QUICKSET(AM_INPUT_CONFIG, buf, .present = 1);
    IOE_QUICKSET(AM_TIMER_UPTIME, buf, .us = ((uint64_t) NDL_GetTicks()) * 1000);
    case AM_INPUT_KEYBRD:
    {
      char buf[64];
      if (0 == NDL_PollEvent(buf, sizeof buf)) {
        *((AM_INPUT_KEYBRD_T *)buf) = (AM_INPUT_KEYBRD_T){.keycode = 0};
        break;
      }
      bool keydown = buf[1] == 'd';
      int find;
      for (find = 0; find < ARRLEN(keynames); find++) {
        if (strncmp(keynames[find], buf + 3, strlen(keynames[find])) == 0)
          break;
      }
      if (find == ARRLEN(keynames)) {
        fprintf(stderr, "Unknown event %s\n", buf);
        assert(0);
      }
      *((AM_INPUT_KEYBRD_T *)buf) =
          (AM_INPUT_KEYBRD_T){.keydown = keydown, .keycode = find};
      break;
    }
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
