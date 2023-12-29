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
#include "memory/paddr.h"

enum {
  reg_freq,
  reg_channels,
  reg_samples,
  reg_sbuf_size,
  reg_init,
  reg_count,
  paddr_start_lo,
  paddr_start_hi,
  paddr_end_lo,
  paddr_end_hi,
  nr_reg
};

static uint32_t *audio_base = NULL;

#define PSEDOBUF_SIZE 0x10000

static void audio_io_handler(uint32_t offset, int len, bool is_write) {
  switch(offset / 4) {
    case reg_freq:
    case reg_channels:
    case reg_samples:
    case reg_sbuf_size:
      break;
    case reg_init:
      SDL_InitSubSystem(SDL_INIT_AUDIO);
      SDL_AudioSpec spec = {
        .freq = audio_base[reg_freq],
        .channels = audio_base[reg_channels],
        .samples = audio_base[reg_samples],
        .format = AUDIO_S16SYS,
        .callback = NULL,
        .userdata = NULL,
      };
      if (0 != SDL_OpenAudio(&spec, NULL)) {
        panic("Failed to open audio: %s\n", SDL_GetError());
      }
      SDL_PauseAudioDevice(1, 0);
      break;
    case reg_count: {
      uint32_t used = SDL_GetQueuedAudioSize(1);
      if (used > PSEDOBUF_SIZE) {
        audio_base[reg_count] = PSEDOBUF_SIZE;
      } else {
        audio_base[reg_count] = used;
      }
      printf("reg_cound: %x\n", audio_base[reg_count]);
      break;
    }
    case paddr_start_lo:
    case paddr_start_hi:
    case paddr_end_lo: break;
    case paddr_end_hi:
    {
      paddr_t start = *(paddr_t *) &audio_base[paddr_start_lo];
      paddr_t end = *(paddr_t *) &audio_base[paddr_end_lo];
      printf("audio: start = %x, end = %x, len = %x\n", start, end, end - start);
      fflush(stdout);
      assert(in_pmem(start) && in_pmem(end) && start <= end);
      SDL_QueueAudio(1, guest_to_host(0x802cc588), end - start);
      break;
    }
    default:
      panic("Unknown audio_io_handler call offset = %x, len = %d, is_write = %d\n", offset, len,
            is_write);
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
  audio_base[reg_sbuf_size] = PSEDOBUF_SIZE;
}
