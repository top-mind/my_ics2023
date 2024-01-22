#include <stdbool.h>
enum { INTR_TIMER, INTR_IODEV, NR_INTR};
extern bool INTR[];
void dev_raise_intr(int NO);
