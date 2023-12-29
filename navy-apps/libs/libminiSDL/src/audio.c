#include <NDL.h>
#include <SDL.h>
#include <assert.h>

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
  assert(desired->format == AUDIO_S16SYS);
  NDL_OpenAudio(desired->freq, desired->channels, desired->samples);
  if (obtained) {
    obtained->freq = desired->freq;
    obtained->format = desired->format;
    obtained->channels = desired->channels;
    obtained->samples = desired->samples;
    obtained->size = desired->samples * 2;
  }
  return 0;
}

void SDL_CloseAudio() {
}

void SDL_PauseAudio(int pause_on) {
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
