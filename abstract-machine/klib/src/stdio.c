#include <am.h>
#include <klib-macros.h>
#include <klib.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

static int buf[4096];

typedef void (*putc_t)(void *, char ch);

int vprintf_internel(const char *fmt, va_list ap, putc_t putc) {
  int data = 0;
  while (*fmt != '\0') {
    if (*fmt == '%') {
parse_loop:
      fmt++;
      switch (*fmt) {
      case 'd': {
        int num = va_arg(ap, int);
        if (num < 0) {
          putc(NULL, '-');
          num = -num;
          data++;
        }
        int len = 0;
        int tmp = num;
        do {
          buf[len++] = tmp % 10;
          tmp /= 10;
        } while (tmp);
        for (int i = len - 1; i >= 0; i--) {
          putc(NULL, buf[i] + '0');
        }
        data += len;
        break;
      }
      case 's': {
        char *str = va_arg(ap, char *);
        while (*str != '\0') {
          putc(NULL, *str++);
          data++;
        }
        break;
      }
      case 'c': {
        char ch = va_arg(ap, int);
        putc(NULL, ch);
        data++;
        break;
      }
      case 'p': {
        putc(NULL, '0');
        putc(NULL, 'x');
        data += 2;
        unsigned int num = va_arg(ap, unsigned int);
        int len = 0;
        unsigned int tmp = num;
        do {
          buf[len++] = tmp % 16;
          tmp /= 16;
        } while (tmp);
        for (int i = len - 1; i >= 0; i--) {
          if (buf[i] < 10) {
            putc(NULL, buf[i] + '0');
          } else {
            putc(NULL, buf[i] - 10 + 'a');
          }
        }
        data += len;
        break;
      }
      case 'x': {
        unsigned int num = va_arg(ap, unsigned int);
        int len = 0;
        unsigned int tmp = num;
        do {
          buf[len++] = tmp % 16;
          tmp /= 16;
        } while (tmp);
        for (int i = len - 1; i >= 0; i--) {
          if (buf[i] < 10) {
            putc(NULL, buf[i] + '0');
          } else {
            putc(NULL, buf[i] - 10 + 'a');
          }
        }
        break;
      }
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        goto parse_loop;
      default: {
        putch('`');
        putstr(fmt);
        putch('\'');
        panic("Not implemented (printf flag, see above)");
        break;
      }
      }
    } else {
      putc(NULL, *fmt);
    }
    fmt++;
  }
  return data;
}

void pputch(void *p, char ch) { putch(ch); }

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int res = vprintf_internel(fmt, ap, pputch);
  va_end(ap);
  return res;
}

void sputch(void *p, char ch) {
  static char *buf = NULL;
  if (p == NULL) {
    *buf++ = ch;
  } else {
    buf = p;
  }
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  // Find the first '%'
  sputch(out, 0);
  int res = vprintf_internel(fmt, ap, sputch);
  sputch(NULL, '\0');
  return res;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsprintf(out, fmt, ap);
  va_end(ap);
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
