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

#include "memory/paddr.h"
#include <common.h>
#include <device/map.h>
#include <SDL2/SDL.h>

enum { reg_freq, reg_channels, reg_samples, reg_sbuf_size, reg_init, reg_count, nr_reg };

static uint8_t *sbuf = NULL;
static uint32_t *audio_base = NULL;

static bool is_audio_sbuf_idle = false;
static int block_size;
static uint32_t __attribute_used__ count_old;
static int __attribute_used__ upd_delay;

#define CONFIG_DELAY 10
#define SBUF_WORK_AS_FIFO

static void audio_io_handler(uint32_t offset, int len, bool is_write) {
  switch (offset / sizeof(uint32_t)) {
    case reg_freq: break;
    case reg_channels: break;
    case reg_samples: break;
    case reg_sbuf_size: assert(!is_write); break;
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
        if (SDL_OpenAudio(&want, NULL)) {
          printf("SDL audio: %s\n", SDL_GetError());
          assert(0);
        }
        SDL_PauseAudioDevice(1, 0);
        is_audio_sbuf_idle = true;
      }
      break;
    case reg_count:
      assert(!is_write);
      assert(is_audio_sbuf_idle);
      uint32_t used = SDL_GetQueuedAudioSize(1);

      /*
      if (used > count_old)
        count_old = used;
      else {
        if (upd_delay == CONFIG_DELAY) {
          count_old = used;
          upd_delay = 0;
        } else {
          used = count_old;
          upd_delay++;
        }
      }
      */
      audio_base[reg_count] = used;
      break;
    default: printf("%d\n", offset); assert(0);
  }
}

static void audio_sbuf_handler(uint32_t offset, int len, bool is_write) {
  assert(is_write);
#ifndef SBUF_WORK_AS_FIFO
  static uint32_t start = 0;
  if (start == 0) {
    start = *(uint32_t *)sbuf;
  } else {
    uint32_t end = *(uint32_t *)sbuf;
    if (0 != SDL_QueueAudio(1, (void *)guest_to_host(start), end - start)) {
      printf("SDL: %s\n", SDL_GetError());
      assert(0);
    }
    start = 0;
  }
#else
  if (offset == 0) {
    is_audio_sbuf_idle = true;
    if (0 != SDL_QueueAudio(1, sbuf, block_size ?: len)) printf("SDL: %s\n", SDL_GetError());
    block_size = 0;
  } else {
    if (is_audio_sbuf_idle) {
      block_size = offset + len;
      is_audio_sbuf_idle = false;
    }
  }
#endif
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
