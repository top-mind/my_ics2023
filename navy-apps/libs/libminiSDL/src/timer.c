#include <NDL.h>
#include <sdl-timer.h>
#include <sdl-event.h>
#include <sdl-audio.h>
#include <stdio.h>
#include <assert.h>

SDL_TimerID SDL_AddTimer(uint32_t interval, SDL_NewTimerCallback callback, void *param) {
  assert(0);
  return NULL;
}

int SDL_RemoveTimer(SDL_TimerID id) {
  return 1;
}

uint32_t SDL_GetTicks() {
  printf("SDL_GetTicks\n");
  return NDL_GetTicks();
}

void SDL_Delay(uint32_t ms) {
  uint32_t time = NDL_GetTicks() + ms;
  while (NDL_GetTicks() < time) {
    SDL_PollEvent(NULL);
  }
}
