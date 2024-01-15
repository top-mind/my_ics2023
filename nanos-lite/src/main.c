#include <common.h>

void init_mm(void);
void init_device(void);
void init_ramdisk(void);
void init_irq(void);
void init_fs(void);
void init_proc(void);

char *__mainargs;

int main(char *mainargs) {
  __mainargs = mainargs;
  extern const char logo[];
  printf("%s", logo);
  Log("'Hello World!' from Nanos-lite");
  Log("Build time: %s, %s", __TIME__, __DATE__);

  init_mm();

  init_device();

  init_ramdisk();

#ifdef HAS_CTE
  init_irq();
#endif

  init_fs();
  void *new_page(size_t);
  void *p = new_page(1);
  printf("%p\n", p);
  for (int i = 0; i < 10; i++) {
    printf("page(i): %p\n", new_page(i));
  }
  return 0;

  init_proc();

  Log("Finish initialization");

#ifdef HAS_CTE
  yield();
#endif

  panic("Should not reach here");
}
