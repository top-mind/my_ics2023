#include <am.h>
#include <nemu.h>
#include <klib.h>

#define SYNC_ADDR (VGACTL_ADDR + 4)

void __am_gpu_init() {
}

static int W = 0, H = 0;

void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  if (W == 0) {
    uint32_t screen_info = inl(VGACTL_ADDR);
    W = screen_info >> 16;
    H = screen_info & 0xffff;
  }
  *cfg = (AM_GPU_CONFIG_T) {
    .present = true, .has_accel = false,
    .width = W, .height = H,
    .vmemsz = W * H * 4,
  };
}

void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  return;
  int x = ctl->x, y = ctl->y, w = ctl->w, h = ctl->h;
  // we assume W = 400
  for (int i = x; i < x + w; i++) {
    for (int j = y; j < y + h; j++) {
      outl(FB_ADDR + (i + j * 400) * 4, ((uint32_t *)ctl->pixels)[(i - x) + (j - y) * w]);
    }
  }
  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
}

void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = true;
}
