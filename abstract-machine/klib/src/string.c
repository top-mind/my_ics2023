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
  while (*src != '\0') {
    *tmp++ = *src++;
  }
  *tmp = '\0';
  return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
  panic("Not implemented");
}

char *strcat(char *dst, const char *src) {
  char *tmp = dst;
  while (*tmp != '\0')
    tmp++;
  while (*src != '\0') {
    *dst++ = *src++;
  }
  *dst = '\0';
  return dst;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 != '\0' && *s2 != '\0') {
    if (*s1 != *s2)
      return (unsigned char) *s1 < (unsigned char) *s2 ? -1 : 1;
    s1++, s2++;
  }
  return 0;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  panic("Not implemented");
}

void *memset(void *s, int c, size_t n) {
  panic("Not implemented");
}

void *memmove(void *dst, const void *src, size_t n) {
  panic("Not implemented");
}

void *memcpy(void *out, const void *in, size_t n) {
  panic("Not implemented");
}

int memcmp(const void *s1, const void *s2, size_t n) {
  panic("Not implemented");
}

#endif
