#include <NDL.h>
#include <malloc.h>

int SDL_Init(uint32_t flags) {
  return NDL_Init(flags);
}

void SDL_Quit() {
  NDL_Quit();
  extern uint32_t *gbPixels;
  if (gbPixels) free(gbPixels);
}

char *SDL_GetError() {
  return "Navy does not support SDL_GetError()";
}

int SDL_SetError(const char* fmt, ...) {
  return -1;
}

int SDL_ShowCursor(int toggle) {
  return 0;
}

void SDL_WM_SetCaption(const char *title, const char *icon) {
}
