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

#include "debug.h"
#include "memory/vaddr.h"
#include <SDL2/SDL_audio.h>
#include <common.h>
#include <device/map.h>
#include <SDL2/SDL.h>
#include <isa.h>
#include <memory/paddr.h>

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
uint8_t bufpage[PAGE_SIZE];

// Do not change the value of AUDIO_DELAY
#define AUDIO_DELAY 0
#if AUDIO_DELAY != 0
static int delay_count = 0;
static int old_used = 0;
#endif

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
        printf(ANSI_FMT(ANSI_FG_RED, "%s\n"), SDL_GetError());
      }
      SDL_PauseAudioDevice(1, 0);
      break;
    case reg_count: {
      int used = SDL_GetQueuedAudioSize(1);
#if AUDIO_DELAY != 0
      if (old_used < used || delay_count == AUDIO_DELAY) {
        delay_count = 0;
        old_used = used;
      } else {
        delay_count++;
        used = old_used;
      }
#endif
      audio_base[reg_count] = used;
      break;
    }
    case paddr_start_lo:
    case paddr_start_hi:
    case paddr_end_lo: break;
    case paddr_end_hi:
    {
      paddr_t start = *(paddr_t *) &audio_base[paddr_start_lo];
      paddr_t end = *(paddr_t *) &audio_base[paddr_end_lo];
      while (SDL_GetQueuedAudioSize(1) + (end - start) > PSEDOBUF_SIZE) {
        SDL_Delay(1);
      }
      if (!isa_mmu_check(start, 4, MEM_TYPE_READ)) {
        SDL_QueueAudio(1, guest_to_host(start), end - start);
      } else {
        if ((start >> PAGE_SHIFT) == (end >> PAGE_SHIFT)) {
          paddr_t pa = isa_mmu_translate(start, 4, MEM_TYPE_READ);
          assert((pa & PAGE_MASK) == MEM_RET_OK);
          memcpy(bufpage, guest_to_host(pa + (start & PAGE_MASK)), end - start);
          SDL_QueueAudio(1, bufpage, end - start);
          break;
        }
        vaddr_t pgstart = ROUNDUP(start, PAGE_SIZE);
        vaddr_t pgend = ROUNDDOWN(end, PAGE_SIZE);
        paddr_t pa = isa_mmu_translate(start, 4, MEM_TYPE_READ);
        assert((pa & PAGE_MASK) == MEM_RET_OK);
        if (pgstart > start) {
          memcpy(bufpage, guest_to_host(pa + (start & PAGE_MASK)), pgstart - start);
          SDL_QueueAudio(1, bufpage, pgstart - start);
        }
        int nr_page = (pgend - pgstart) / PAGE_SIZE;
        for (int i = 0; i < nr_page; i++) {
          pa = isa_mmu_translate(pgstart + i * PAGE_SIZE, 4, MEM_TYPE_READ);
          SDL_QueueAudio(1, guest_to_host(pa), PAGE_SIZE);
        }
        if (pgend < end) {
          pa = isa_mmu_translate(pgend, 4, MEM_TYPE_READ);
          assert((pa & PAGE_MASK) == MEM_RET_OK);
          memcpy(bufpage, guest_to_host(pa), end - pgend);
          SDL_QueueAudio(1, bufpage, end - pgend);
        }
      }
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
