#include <stdlib.h>
#include <stdio.h>
void *xmalloc(size_t bytes) {
  void *ret = malloc(bytes);
  if (ret == NULL) {
    fprintf(stderr, "xmalloc failed\n");
    exit(2);
  }
  return ret;
}
