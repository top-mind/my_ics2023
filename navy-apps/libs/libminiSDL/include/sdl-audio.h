#ifndef __SDL_AUDIO_H__
#define __SDL_AUDIO_H__

typedef struct {
  int freq;
  uint16_t format;
  uint8_t channels;
  uint16_t samples;
  uint32_t size;
  void (*callback)(void *userdata, uint8_t *stream, int len);
  void *userdata;
} SDL_AudioSpec;

// Simulate SDL audio callback

extern int callback_period, callback_size, callback_time, old_time;
extern void AudioHelper();

#define InvokeAudioCallback()                                                  \
  do {                                                                         \
    uint32_t time = NDL_GetTicks();                                            \
    callback_time += (time - old_time);                                        \
    old_time = time;                                                           \
    if (callback_time >= callback_period) {                                    \
      AudioHelper();                                                        \
      callback_time -= callback_period;                                        \
    }                                                                          \
  } while (0)

#define AUDIO_U8 8
#define AUDIO_S16 16
#define AUDIO_S16SYS AUDIO_S16

#define SDL_MIX_MAXVOLUME  128

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
void SDL_CloseAudio();
void SDL_PauseAudio(int pause_on);
SDL_AudioSpec *SDL_LoadWAV(const char *file, SDL_AudioSpec *spec, uint8_t **audio_buf, uint32_t *audio_len);
void SDL_FreeWAV(uint8_t *audio_buf);
void SDL_MixAudio(uint8_t *dst, uint8_t *src, uint32_t len, int volume);
void SDL_LockAudio();
void SDL_UnlockAudio();

#endif
