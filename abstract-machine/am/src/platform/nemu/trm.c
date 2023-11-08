#include <am.h>
#include <nemu.h>

extern char _heap_start;
int main(const char *args);

Area heap = RANGE(&_heap_start, PMEM_END);
#ifndef MAINARGS
#define MAINARGS ""
#endif
// extern const char *_nemu_mainargs;
static const char mainargs[] = MAINARGS;

void putch(char ch) {
  outb(SERIAL_PORT, ch);
}

void halt(int code) {
  nemu_trap(code);
  // should not reach here
  while (1);
}

void _trm_init() {
  int ret = main(mainargs);
  // int ret = main(_nemu_mainargs);
  halt(ret);
}
