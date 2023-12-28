#include <fs.h>
#include <ramdisk.h>
#include <sys_errno.h>

typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);

size_t serial_write(const void *buf, size_t offset, size_t len);
size_t events_read(void *buf, size_t offset, size_t len);
size_t dispinfo_read(void *buf, size_t offset, size_t len);
size_t fb_write(const void *buf, size_t offset, size_t len);
size_t sb_write(const void *buf, size_t offset, size_t len);
size_t get_fbsize();

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
  size_t open_offset;
} Finfo;

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB, FD_EVENT, FD_DISPINFO, FD_SB};
size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("This device does not support read");
  return -1;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("This device does not support write");
  return -1;
}

/* This is the information about all files in disk.
 * Special read/write helpers:
 *   invalid_read, invalid_write: the device is not readable or writable.
 *   NULL, fs_read will call seekable_read, so as write.
 */
static Finfo file_table[] __attribute__((used)) = {
    [FD_STDIN] = {"stdin", 0, 0, invalid_read, invalid_write},
    [FD_STDOUT] = {"stdout", 0, 0, invalid_read, serial_write},
    [FD_STDERR] = {"stderr", 0, 0, invalid_read, serial_write},
    [FD_FB] = {"/dev/fb", 0, 0, invalid_read, NULL},
    [FD_EVENT] = {"/dev/events", 0, 0, events_read, invalid_write},
    [FD_DISPINFO] = {"/proc/dispinfo", 0, 0, dispinfo_read, invalid_write},
    [FD_SB] = {"/dev/sb", 0, 0, invalid_read, sb_write},
#include "files.h"
};

void init_fs() {
  file_table[FD_FB].size = get_fbsize();
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
      return i;
    }
  }
  if (flags & 0x0200) {
    panic("sfs cannot create file %s", pathname);
    return -EACCES;
  }
  return -ENOENT;
}

size_t fs_read(int fd, void *buf, size_t len) {
  if(fd < 0 || fd >= ARRLEN(file_table)) {
    panic("fd %d not exist", fd);
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

static size_t seekable_write(int fd, const void *buf, size_t len) {
  size_t t = len;
  if (file_table[fd].open_offset + len > file_table[fd].size) {
    t = file_table[fd].size - file_table[fd].open_offset;
  }
  size_t ret;
  switch (fd) {
    case FD_FB:
      ret = fb_write(buf, file_table[fd].disk_offset + file_table[fd].open_offset, t);
      break;
    default:
      ret = ramdisk_write(
          buf, file_table[fd].disk_offset + file_table[fd].open_offset, t);
  }
  assert(ret == t);
  file_table[fd].open_offset += ret;
  return ret;
}

size_t fs_write(int fd, const void *buf, size_t len) {
  if(fd < 0 || fd >= ARRLEN(file_table)) {
    panic("fd %d not exist", fd);
    return -EBADF;
  }
  if (file_table[fd].write)
    return file_table[fd].write(buf, file_table[fd].disk_offset + file_table[fd].open_offset, len);
  return seekable_write(fd, buf, len);
}

size_t fs_lseek(int fd, size_t offset, int whence) {
  if(fd < 0 || fd >= ARRLEN(file_table)) {
    panic("fd %d not exist", fd);
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

