#include <common.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
# define MULTIPROGRAM_YIELD() yield()
#else
# define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) \
  [AM_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [AM_KEY_NONE] = "NONE",
  AM_KEYS(NAME)
};

static bool has_uart = 0;
static bool has_key = 0;

static int screen_w, screen_h;
static size_t screen_size;

static int sbsize = 0;

size_t serial_write(const void *buf, size_t offset, size_t len) {
  // yield();
  size_t i;
  for (i = 0; i < len; i++)
    putch(((char *)buf)[i]);
  return i;
}

size_t events_read(void *buf, size_t offset, size_t len) {
  // yield();
  if (has_uart)
    ; /* not implemented */
  if (has_key) {
    AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
    if (ev.keycode != AM_KEY_NONE) {
      return snprintf(buf, len, "k%c %s\n", ev.keydown ? 'd' : 'u', keyname[ev.keycode]);
    }
  }
  return 0;
}

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  return snprintf(buf, len, "WIDTH :%d\nHEIGHT:%d\n", screen_w, screen_h);
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  assert((offset & 3) == 0 && (len & 3) == 0);
  // yield();
  int x = offset / 4 % screen_w;
  int y = offset / 4 / screen_w;
  io_write(AM_GPU_FBDRAW, x, y, (void *)buf, len / 4, 1, 1);
  return len;
}

size_t sb_write(const void *buf, size_t offset, size_t len) {
  io_write(AM_AUDIO_PLAY,
           (Area){.start = (void *)buf, .end = (void *)buf + len});

  return len;
}

size_t sbctl_write(const void *buf, size_t offset, size_t len) {
  assert(len == 12);
  io_write(AM_AUDIO_CTRL, .freq = *(int *)buf, .channels = *(int *)(buf + 4), .samples = *(int *)(buf + 8));
  return len;
}

size_t sbctl_read(void *buf, size_t offset, size_t len) {
  return snprintf(buf, len, "%d", sbsize - io_read(AM_AUDIO_STATUS).count);
}

int get_fbsize() {
  return screen_size;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
  has_key = io_read(AM_INPUT_CONFIG).present;
  if (has_key)
    Log("Input device has been detected!");
  AM_GPU_CONFIG_T info = io_read(AM_GPU_CONFIG);
  screen_w = info.width;
  screen_h = info.height;
  screen_size = screen_w * screen_h * sizeof(uint32_t);
  Log("Initializing Screen, size: %d x %d\n", screen_w, screen_h);
  sbsize = io_read(AM_AUDIO_CONFIG).bufsize;
  Log("Initializing Audio, buffer size: %d\n", sbsize);
}
