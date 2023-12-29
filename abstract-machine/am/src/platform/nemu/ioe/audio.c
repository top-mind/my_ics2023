#include <am.h>
#include <nemu.h>
#include <klib.h>

#define AUDIO_FREQ_ADDR      (AUDIO_ADDR + 0x00)
#define AUDIO_CHANNELS_ADDR  (AUDIO_ADDR + 0x04)
#define AUDIO_SAMPLES_ADDR   (AUDIO_ADDR + 0x08)
#define AUDIO_SBUF_SIZE_ADDR (AUDIO_ADDR + 0x0c)
#define AUDIO_INIT_ADDR      (AUDIO_ADDR + 0x10)
#define AUDIO_COUNT_ADDR     (AUDIO_ADDR + 0x14)
#define AUDIO_PLAY     (AUDIO_ADDR + 0x18)

void __am_audio_init() {
}

void __am_audio_config(AM_AUDIO_CONFIG_T *cfg) {
  cfg->present = true;
  cfg->bufsize = 0x1000;
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

/* Block function
 * make sure [ctl->start, ctl->end) is appended in sound buffer
 */
void __am_audio_play(AM_AUDIO_PLAY_T *ctl) {
  uintptr_t start = (uintptr_t)ctl->buf.start;
  uintptr_t end = (uintptr_t)ctl->buf.end;
  outl(AUDIO_PLAY, start & 0xffffffff);
  outl(AUDIO_PLAY + 4, ((uint64_t) start) >> 32);
  outl(AUDIO_PLAY + 8, end & 0xffffffff);
  outl(AUDIO_PLAY + 12, ((uint64_t) end) >> 32);
}
