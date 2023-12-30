#include <NDL.h>
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int callback_period, callback_size, callback_time, old_time;

void (*callback)(void *userdata, uint8_t *stream, int len);
void *userdata;

static uint8_t *audio_buf;
static bool audio_playing, flag_callback;

void CallbackHelper() {
  if (!audio_playing || flag_callback) return;
  // forbid recursive callback
  flag_callback = 1;
  callback(userdata, audio_buf, callback_size);
  NDL_PlayAudio(audio_buf, callback_size);
  flag_callback = 0;
}

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
  assert(desired->format == AUDIO_S16SYS);
  NDL_OpenAudio(desired->freq, desired->channels, desired->samples);
  callback = desired->callback;
  userdata = desired->userdata;
  if (obtained) {
    memmove(obtained, desired, sizeof(SDL_AudioSpec));
    desired = obtained;
  }
  desired->size = callback_size =
      desired->samples * desired->channels * sizeof(uint16_t);
  audio_buf = malloc(callback_size);
  callback_period = 1000 * desired->samples / desired->freq;
  return 0;
}

void SDL_CloseAudio() {
  free(audio_buf);
  NDL_CloseAudio();
}

void SDL_PauseAudio(int pause_on) {
  audio_playing = !pause_on;
}

void SDL_MixAudio(uint8_t *dst, uint8_t *src, uint32_t len, int volume) {
  int16_t *d = (int16_t *)dst, *s = (int16_t *)src;
  int i;
  for (i = 0; i < len / 2; i++) {
    int32_t t = d[i] + s[i] * volume / SDL_MIX_MAXVOLUME;
    if (t > 32767) t = 32767;
    if (t < -32768) t = -32768;
    d[i] = t;
  }
}


SDL_AudioSpec *SDL_LoadWAV(const char *file, SDL_AudioSpec *spec, uint8_t **audio_buf, uint32_t *audio_len) {
  if (file == NULL || spec == NULL || audio_buf == NULL || audio_len == NULL)
    return NULL;
  FILE *f = fopen(file, "r");
  if (!f)
    return NULL;
  struct WAVEHeader {
    char riff[4];
    uint32_t size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    struct {
      uint16_t format;
      uint16_t channels;
      uint32_t sample_rate;
      uint32_t byte_rate;
      uint16_t block_align;
      uint16_t bits_per_sample;
    } fmt_data;
    char data[4];
    uint32_t data_size;
  } header;
  if (fread(&header, 1, sizeof header, f) != sizeof header) {
    fclose(f);
    return NULL;
  }
  if (memcmp(header.riff, "RIFF", 4) || memcmp(header.wave, "WAVE", 4) ||
      memcmp(header.fmt, "fmt ", 4) || memcmp(header.data, "data", 4) ||
      header.fmt_data.format != 1 || header.fmt_data.bits_per_sample != 16) {
    fclose(f);
    return NULL;
  }
  memset(spec, 0, sizeof *spec);
  spec->freq = header.fmt_data.sample_rate;
  spec->format = AUDIO_S16SYS;
  spec->channels = header.fmt_data.channels;
  spec->samples = 1024;

  *audio_len = header.data_size;
  *audio_buf = malloc(header.data_size);
  if (fread(*audio_buf, 1, header.data_size, f) != header.data_size) {
    fclose(f);
    return NULL;
  }
  fclose(f);
  return spec;
}

void SDL_FreeWAV(uint8_t *audio_buf) {
  free(audio_buf);
}

void SDL_LockAudio() {
}

void SDL_UnlockAudio() {
}
