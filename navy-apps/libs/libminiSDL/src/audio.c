#include <NDL.h>
#include <SDL.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int callback_period, callback_size, callback_time;

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
}

SDL_AudioSpec *SDL_LoadWAV(const char *file, SDL_AudioSpec *spec, uint8_t **audio_buf, uint32_t *audio_len) {
  return NULL;
}

void SDL_FreeWAV(uint8_t *audio_buf) {
}

void SDL_LockAudio() {
}

void SDL_UnlockAudio() {
}
