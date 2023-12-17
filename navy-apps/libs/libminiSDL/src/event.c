#include <NDL.h>
#include <SDL.h>
#include <assert.h>
#include <string.h>

#define keyname(k) #k,

static const char *keyname[] = {
  "NONE",
  _KEYS(keyname)
};

static inline uint8_t find_keyname(const char *name) {
  for (int i = 0; i < sizeof(keyname) / sizeof(keyname[0]); i ++) {
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
  }
  return ret;
}

int SDL_WaitEvent(SDL_Event *event) {
  while (SDL_PollEvent(event))
    ;
  return 1;
}

int SDL_PeepEvents(SDL_Event *ev, int numevents, int action, uint32_t mask) {
  return 0;
}

uint8_t* SDL_GetKeyState(int *numkeys) {
  return NULL;
}
