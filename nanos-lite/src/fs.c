#include <fs.h>

typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);

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

size_t serial_write(const void *buf, size_t offset, size_t len);

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]  = {"stdin" , 0, 0, invalid_read, invalid_write},
  [FD_STDOUT] = {"stdout", 0, 0, invalid_read, serial_write},
  [FD_STDERR] = {"stderr", 0, 0, invalid_read, serial_write},
#include "files.h"
};

const char* fs_get_name(int fd) {
  return file_table[fd].name;
}

void init_fs() {
  // TODO: initialize the size of /dev/fb
}

int fs_open(const char *pathname)
{
  for (int i = 0; i < sizeof(file_table) / sizeof(file_table[0]); i++) {
    if (strcmp(pathname, file_table[i].name) == 0) {
      file_table[i].open_offset = 0;
      return i;
    }
  }
  panic("File %s not found\n", pathname);
}

size_t fs_read(int fd, void *buf, size_t len)
{
  Finfo *f = &file_table[fd];
  if (f->read != NULL) {
    return f->read(buf, f->open_offset, len);
  }
  if (f->open_offset >= f->size) return 0;
  if (len + f->open_offset > f->size) {
    len = f->size - f->open_offset;
  }
  size_t offset = f->disk_offset + f->open_offset;
  f->open_offset += len;
  return ramdisk_read(buf, offset, len);
}
int fs_close(int fd) {
  return 0;
}
size_t fs_lseek(int fd, intptr_t offset, int whence) {
  Finfo *f = &file_table[fd];
  intptr_t new_offset = f->open_offset;
  switch(whence) {
  case SEEK_SET: new_offset = offset; break;
  case SEEK_CUR: new_offset += offset; break;
  case SEEK_END: new_offset = f->size + offset; break;
  default: return -1;
  }
  if (new_offset < 0) {
    new_offset = 0;
  } else if (new_offset > f->size) {
    new_offset = f->size;
  }
  f->open_offset = new_offset;
  return f->open_offset;
}
size_t fs_write(int fd, const void *buf, size_t len) {
  Finfo *f = &file_table[fd];
  if (f->write != NULL) {
    return f->write(buf, f->open_offset, len);
  }
  if (f->open_offset >= f->size) {
    return 0;
  }
  if (f->open_offset + len > f->size) {
    len = f->size - f->open_offset;
  }
  ramdisk_write(buf, f->disk_offset + f->open_offset, len);
  f->open_offset += len;
  return len;
}
