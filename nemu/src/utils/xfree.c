#include <common.h>

void xfree(void *p) {
  if (p)
    free(p);
}
