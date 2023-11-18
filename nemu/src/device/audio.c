/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <SDL2/SDL_audio.h>
#include <common.h>
#include <device/map.h>
#include <SDL2/SDL.h>

enum { reg_freq, reg_channels, reg_samples, reg_sbuf_size, reg_init, reg_count, nr_reg };

static uint8_t *sbuf = NULL;
static uint32_t *audio_base = NULL;

static int queue_head = 0, queue_tail = 0;
static bool valid_count = false;
static uint8_t sdl_silence = 0;
static uint32_t sdl_size = 0;

static void audio_io_handler(uint32_t offset, int len, bool is_write) {
  switch(offset / sizeof(uint32_t)) {
    case reg_freq:
      break;
    case reg_channels:
      break;
    case reg_samples:
      break;
    case reg_sbuf_size:
      assert(!is_write);
      break;
    case reg_init:
      if (is_write) {
        // init SDL audio
        SDL_AudioSpec want = {
          .callback = NULL,
          .channels = audio_base[reg_channels],
          .format = AUDIO_S16SYS,
          .freq = audio_base[reg_freq],
          .samples = audio_base[reg_samples],
          .userdata = NULL,
        };
        SDL_OpenAudio(&want, NULL);
        sdl_silence = want.silence;
        sdl_size = want.size;
        valid_count = true;
      }
      break;
    case reg_count:
      assert(!is_write);
      assert(valid_count);
      int used = (queue_tail + CONFIG_SB_SIZE - queue_head) % CONFIG_SB_SIZE;
      audio_base[reg_count] = used + 1;
      break;
    default:
      printf("%d\n", offset);
      assert(0);
  }
}

static void audio_sbuf_handler(uint32_t offset, int len, bool is_write) {
  assert(is_write);
  if (offset == 0) {
    valid_count = true;
    SDL_QueueAudio(1, sbuf, queue_tail ? queue_tail : len);
    printf("nemu: audio queue push %d\n", len);
    queue_tail = 0;
  } else {
    if (valid_count) {
      queue_tail = offset + len;
      valid_count = false;
    }
  }
}

void init_audio() {
  uint32_t space_size = sizeof(uint32_t) * nr_reg;
  audio_base = (uint32_t *)new_space(space_size);
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map("audio", CONFIG_AUDIO_CTL_PORT, audio_base, space_size, audio_io_handler);
#else
  add_mmio_map("audio", CONFIG_AUDIO_CTL_MMIO, audio_base, space_size, audio_io_handler);
#endif
  memset(audio_base, 0, space_size);
  audio_base[reg_sbuf_size] = CONFIG_SB_SIZE;
  sbuf = (uint8_t *)new_space(CONFIG_SB_SIZE);
  add_mmio_map("audio-sbuf", CONFIG_SB_ADDR, sbuf, CONFIG_SB_SIZE, audio_sbuf_handler);
  memset(sbuf, 0, CONFIG_SB_SIZE);
}
