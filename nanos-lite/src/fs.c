#include <fs.h>
#include <ramdisk.h>
#include <sys_errno.h>

typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);

size_t serial_write(const void *buf, size_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
  size_t open_offset;
} Finfo;

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB};

size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]  = {"stdin", 0, 0, invalid_read, invalid_write},
  [FD_STDOUT] = {"stdout", 0, 0, invalid_read, serial_write},
  [FD_STDERR] = {"stderr", 0, 0, invalid_read, serial_write},
#include "files.h"
};

void init_fs() {
  // TODO: initialize the size of /dev/fb
}

/* As linux and glibc's specification, if the return value is between -4095 and -1,
 * it indicates error number.
 */

int fs_open(const char *pathname, int flags, int mode) {
  if (flags & 0x0008) {
    panic("sfs does not support O_APPEND");
    return -EINVAL;
  }
  for (int i = 0; i < ARRLEN(file_table); i++) {
    if (strcmp(pathname, file_table[i].name) == 0) {
      file_table[i].open_offset = 0;
      Log("fs_open: open file %s with fd = %d", pathname, i);
      return i;
    }
  }
  panic("sfs cannot create file %s", pathname);
  if (flags & 0x0200) {
    return -EACCES;
  }
  return -ENOENT;
}

size_t fs_read(int fd, void *buf, size_t len) {
  if(fd < 0 || fd >= ARRLEN(file_table)) {
    panic("fd %d out of range", fd);
    return -EBADF;
  }
  if (file_table[fd].read)
    return file_table[fd].read(buf, file_table[fd].disk_offset + file_table[fd].open_offset, len);
  size_t t = len;
  if (file_table[fd].open_offset + len > file_table[fd].size) {
    t = file_table[fd].size - file_table[fd].open_offset;
  }
  size_t ret = ramdisk_read(buf, file_table[fd].disk_offset + file_table[fd].open_offset, t);
  assert(ret == t);
  file_table[fd].open_offset += ret;
  return ret;
}

size_t fs_write(int fd, const void *buf, size_t len) {
  printf("fs_write: fd = %d, len = %d\n", fd, len);
  if(fd < 0 || fd >= ARRLEN(file_table)) {
    panic("fd %d out of range", fd);
    return -EBADF;
  }
  if (file_table[fd].write)
    return file_table[fd].write(buf, file_table[fd].disk_offset + file_table[fd].open_offset, len);
  size_t t = len;
  if (file_table[fd].open_offset + len > file_table[fd].size) {
    t = file_table[fd].size - file_table[fd].open_offset;
  }
  size_t ret = ramdisk_write(buf, file_table[fd].disk_offset + file_table[fd].open_offset, t);
  assert(ret == t);
  file_table[fd].open_offset += ret;
  return ret;
}

size_t fs_lseek(int fd, size_t offset, int whence) {
  if(fd < 0 || fd >= ARRLEN(file_table)) {
    panic("fd %d out of range", fd);
    return -EBADF;
  }
  switch (whence) {
    case SEEK_SET:
      file_table[fd].open_offset = offset;
      break;
    case SEEK_CUR:
      file_table[fd].open_offset += offset;
      break;
    case SEEK_END:
      file_table[fd].open_offset = file_table[fd].size + offset;
      break;
    default:
      panic("invalid whence");
      return -EINVAL;
  }
  if (file_table[fd].open_offset > file_table[fd].size) {
    panic("fs_lseek: seek beyond file size");
    return -ESPIPE;
  }
  return file_table[fd].open_offset;
}

