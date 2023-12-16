#include "sys/time.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;

uint32_t NDL_GetTicks() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int NDL_PollEvent(char *buf, int len) {
  int ret = read(evtdev, buf, len);
  // to make it easy, panic when buffer is not enough
  // this mean a event is lost
  assert(ret != -1 && ret < len);
  return ret;
}

void NDL_OpenCanvas(int *w, int *h) {
  if (getenv("NWM_APP")) {
    int fbctl = 4;
    fbdev = 5;
    screen_w = *w; screen_h = *h;
    char buf[64];
    int len = sprintf(buf, "%d %d", screen_w, screen_h);
    // let NWM resize the window and create the frame buffer
    write(fbctl, buf, len);
    while (1) {
      // 3 = evtdev
      int nread = read(3, buf, sizeof(buf) - 1);
      if (nread <= 0) continue;
      buf[nread] = '\0';
      if (strcmp(buf, "mmap ok") == 0) break;
    }
    close(fbctl);
  } else {
    
  }
}

void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
}

void NDL_OpenAudio(int freq, int channels, int samples) {
}

void NDL_CloseAudio() {
}

int NDL_PlayAudio(void *buf, int len) {
  return 0;
}

int NDL_QueryAudio() {
  return 0;
}

void init_event() {
  evtdev = open("/dev/events", 0);
  assert(evtdev != -1);
}

void init_display() {
  fbdev = open("/dev/fb", 0);
  assert(fbdev != -1);
  int dispinfo = open("/proc/dispinfo", 0);
  assert(dispinfo != -1);
  char buf[64];
  char *tmp;
  int nread = read(dispinfo, buf, sizeof(buf));
  assert(nread > 0 && nread < sizeof(buf));
  for (tmp = strtok(buf, "\n"); tmp; tmp = strtok(NULL, "\n")) {
    if (1 == sscanf(tmp, "WIDTH : %d", &screen_w)) continue;
    if (1 == sscanf(tmp, "HEIGHT : %d", &screen_h)) continue;
    fprintf(stderr, "/proc/dispinfo syntax error near %s\n", tmp);
    assert(0);
  }
  printf("NDL: init display %d x %d\n", screen_w, screen_h);
}

int NDL_Init(uint32_t flags) {
  if (getenv("NWM_APP")) {
    evtdev = 3;
  } else {
    init_event();
    init_display();
  }
  return 0;
}

void NDL_Quit() {
  if (getenv("NWM_APP")) {
  } else {
    close(evtdev);
    close(fbdev);
  }
}
