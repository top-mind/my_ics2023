#include <am.h>
#include <nemu.h>
#include <klib.h>

#define AUDIO_FREQ_ADDR      (AUDIO_ADDR + 0x00)
#define AUDIO_CHANNELS_ADDR  (AUDIO_ADDR + 0x04)
#define AUDIO_SAMPLES_ADDR   (AUDIO_ADDR + 0x08)
#define AUDIO_SBUF_SIZE_ADDR (AUDIO_ADDR + 0x0c)
#define AUDIO_INIT_ADDR      (AUDIO_ADDR + 0x10)
#define AUDIO_COUNT_ADDR     (AUDIO_ADDR + 0x14)

void __am_audio_init() {
}

void __am_audio_config(AM_AUDIO_CONFIG_T *cfg) {
  cfg->present = true;
  cfg->bufsize = 0x10000;
}

void __am_audio_ctrl(AM_AUDIO_CTRL_T *ctrl) {
  outl(AUDIO_FREQ_ADDR, ctrl->freq);
  outl(AUDIO_CHANNELS_ADDR, ctrl->channels);
  outl(AUDIO_SAMPLES_ADDR, ctrl->samples);
  outl(AUDIO_INIT_ADDR, 1);
}

void __am_audio_status(AM_AUDIO_STATUS_T *stat) {
  stat->count = inl(AUDIO_COUNT_ADDR);
}

void __am_audio_play(AM_AUDIO_PLAY_T *ctl) {
  uintptr_t start = (uintptr_t)ctl->buf.start;
  intptr_t len = (uintptr_t)ctl->buf.end - (uintptr_t)ctl->buf.start;
  // printf("%p %p\n", ctl->buf.start, ctl->buf.end);
  if (len < 0)
    return;
  else {
    printf("%d\n", len);
  }
  for (int i = 0; i < len; i += 4) {
    outl(AUDIO_SBUF_ADDR, *(uint32_t *)(start + i));
  }
  return;
  // aligned write
  if (len & 1) {
    len = len - 1;
    outb(AUDIO_SBUF_ADDR + len, *(uint8_t *)(start + len));
  }
  if (len & 3) {
    len = len - 2;
    outw(AUDIO_SBUF_ADDR + len, *(uint16_t *)(start + len));
  }
  if (len == 0)
    return;
  for (int i = len - 4; i >= 0; i -= 4) {
    outl(AUDIO_SBUF_ADDR + i, *(uint32_t *)(start + i));
  }
}
