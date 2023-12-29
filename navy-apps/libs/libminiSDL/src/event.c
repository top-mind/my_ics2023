#include <NDL.h>
#include <SDL.h>
#include <assert.h>
#include <string.h>

#define keyname(k) #k,

#define def_keystate(k) [k] = 1,

static const char *keyname[] = {
  "NONE",
  _KEYS(keyname)
};

static uint8_t keystate[SDLK_LAST];

static inline uint8_t find_keyname(char *name) {
  strtok(name, "\n");
  for (int i = 0; i < sizeof(keyname) / sizeof(keyname[0]); i++) {
    if (strcmp(keyname[i], name) == 0) {
      return i;
    }
  }
  assert(0);
  return 0;
}

int SDL_PushEvent(SDL_Event *ev) {
  return 0;
}

int SDL_PollEvent(SDL_Event *ev) {
  assert(ev);
  char buf[32];
  int ret = NDL_PollEvent(buf, sizeof(buf));
  if (ret) {
    ev->type = buf[1] == 'u' ? SDL_KEYUP : SDL_KEYDOWN;
    ev->key.keysym.sym = find_keyname(buf + 3);
    keystate[ev->key.keysym.sym] = ev->type == SDL_KEYDOWN;
  }
  uint32_t time = NDL_GetTicks();
  if (time - callback_time >= callback_period) {
    CallbackHelper();
    callback_time = time;
  }
  return ret;
}

int SDL_WaitEvent(SDL_Event *event) {
  while (SDL_PollEvent(event) == 0)
    ;
  return 1;
}

int SDL_PeepEvents(SDL_Event *ev, int numevents, int action, uint32_t mask) {
  assert(0);
  return 0;
}

uint8_t* SDL_GetKeyState(int *numkeys) {
  return keystate;
}
