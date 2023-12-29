#include <stdint.h>
#include <stdio.h>
#include <NDL.h>
#include <assert.h>
#include <stdlib.h>

int main() {
  FILE  *f = fopen("/share/files/littlestar", "r");
  fseek(f, 0, SEEK_END);
  int len = ftell(f);
  uint8_t *buf = malloc(len);
  fseek(f, 0, SEEK_SET);
  assert(len == fread(buf, 1, len, f));
  NDL_Init(0);
  NDL_OpenAudio(8000, 1, 1024);
  NDL_PlayAudio(buf, len);
  int rest = 0;
  while ((rest = NDL_QueryAudio()) > 0) {
    printf("rest = %d\n", rest);
  }
  free(buf);
}
