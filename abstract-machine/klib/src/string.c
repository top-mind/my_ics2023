#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  const char *pend = s;
  while (*pend != '\0') {
    pend++;
  }
  return pend - s;
}

char *strcpy(char *dst, const char *src) {
  char *tmp = dst;
  while (*src != '\0') *tmp++ = *src++;
  *tmp = '\0';
  return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
  char *tmp = dst;
  while (n-- && *src != '\0') *tmp++ = *src++;
  while (n--) *tmp++ = '\0';
  return dst;
}

char *strcat(char *dst, const char *src) {
  char *tmp = dst;
  while (*tmp != '\0') tmp++;
  while (*src != '\0') *tmp++ = *src++;
  *tmp = '\0';
  return dst;
}

int strcmp(const char *s1, const char *s2) {
// linux/lib/string.c
  unsigned char c1, c2;
  while (1) {
    c1 = *s1++;
    c2 = *s2++;
    if (c1 != c2) {
      return c1 < c2 ? -1 : 1;
    }
    if (!c1) {
      break;
    }
  }
  return 0;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  unsigned char c1, c2;
  while (n--) {
    c1 = *s1++;
    c2 = *s2++;
    if (c1 != c2) {
      return c1 < c2 ? -1 : 1;
    }
    if (!c1) {
      break;
    }
  }
  return 0;
}

void *memset(void *s, int c, size_t n) {
  for (size_t i = 0; i < n; i++) {
    *((uint8_t*) s + i) = c;
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  uintptr_t d = (uintptr_t) dst;
  uintptr_t s = (uintptr_t) src;
  if (d < s) {
    while (n--)
      *(uint8_t *)dst++ = *(uint8_t *)src++;
  } else if (d > s) {
    while (n--)
      *((uint8_t *)dst + n) = *((uint8_t *)src + n);
  }
  return (void *) d;
}

void *memcpy(void *out, const void *in, size_t n) {
  void *ret = out;
  while (n--) {
    *(uint8_t *) out++ = *(uint8_t *) in++;
  }
  return ret;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  uint8_t v1, v2;
  while (n--) {
    v1 = *(uint8_t *) s1++;
    v2 = *(uint8_t *) s2++;
    if (v1 != v2) {
      return v1 < v2 ? -1 : 1;
    }
  }
  return 0;
}

#endif
