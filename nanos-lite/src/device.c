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

static inline int
__attribute__((format(printf, 3, 4)))
snprintf0(char *buf, size_t size, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(buf, size, fmt, ap);
  va_end(ap);
  return ret > size - 1 ? size - 1 : ret;
}

size_t serial_write(const void *buf, size_t offset, size_t len) {
  size_t i;
  for (i = 0; i < len; i++)
    putch(((char *)buf)[i]);
  return i;
}

size_t events_read(void *buf, size_t offset, size_t len) {
  if (has_uart)
    ; /* not implemented */
  if (has_key) {
    AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
    if (ev.keycode != AM_KEY_NONE) {
      return snprintf0(buf, len, "k%c %s", ev.keydown ? 'd' : 'u', keyname[ev.keycode]);
    }
  }
  return 0;
}

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  return snprintf0(buf, len, "WIDTH: %d\nHEIGHT: %d", screen_w, screen_h);
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  return 0;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
  has_uart = io_read(AM_UART_CONFIG).present;
  assert(!has_uart);
  has_key = io_read(AM_INPUT_CONFIG).present;
  if (has_key)
    Log("Input device has been detected!");
  AM_GPU_CONFIG_T info = io_read(AM_GPU_CONFIG);
  screen_w = info.width;
  screen_h = info.height;
  Log("Initializing Screen, size: %d x %d\n", screen_w, screen_h);
}
