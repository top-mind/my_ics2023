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
  // AM_DEVREG(11, GPU_FBDRAW,   WR, int x, y; void *pixels; int w, h; bool sync);
  int x = ctl->x, y = ctl->y, w = ctl->w, h = ctl->h;
  if (W == 0)
    panic("Please call `AM_GPUU_CONFIG` first!");
  uint32_t c = *(uint32_t *)ctl->pixels;
  for (int i = x; i < x + w; i++) {
    for (int j = y; j < y + h; j++) {
      outl(FB_ADDR + (i + j * W) * 4, c);
    }
  }
  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
}

void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = true;
}
