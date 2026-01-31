#include <common.h>
#include "syscall.h"
#include <memory.h>
#include <sys/time.h>
#ifdef STRACE
static const char *syscall_names[] = {
  "exit", "yield", "open", "read", "write", "kill", "getpid", "close",
  "lseek", "brk", "fstat", "time", "signal", "execve", "fork", "link",
  "unlink", "wait", "times", "gettimeofday"
};
#endif

int fs_open(const char *pathname);
size_t fs_read(int fd, void *buf, size_t len);
size_t fs_write(int fd, const void *buf, size_t len);
size_t fs_lseek(int fd, intptr_t offset, int whence);
int fs_close(int fd);
const char* fs_get_name(int fd);

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1; // 系统调用号
  a[1] = c->GPR2; // 参数 1
  a[2] = c->GPR3; // 参数 2
  a[3] = c->GPR4; // 参数 3

#ifdef STRACE
  if (a[0] == SYS_open) {
    Log("[strace] open(\"%s\", %p, %p)", (char *)a[1], (void *)a[2], (void *)a[3]);
  } else if (a[0] == SYS_read || a[0] == SYS_write || a[0] == SYS_lseek || a[0] == SYS_close) {
    Log("[strace] %s(fd:%d(%s), %p, %p)", syscall_names[a[0]], (int)a[1], fs_get_name(a[1]), (void *)a[2], (void *)a[3]);
  } else if (a[0] != SYS_write || a[3] > 1) {
    Log("[strace] %s(%p, %p, %p)", syscall_names[a[0]], (void *)a[1], (void *)a[2], (void *)a[3]);
  }
#endif

  switch (a[0]) {
    case SYS_exit:
      Log("[strace] program exit with code %d", a[1]);
      halt(a[1]);
      break;
    case SYS_yield:
      yield();
      c->GPRx = 0;
      break;
    case SYS_open:
      c->GPRx = fs_open((char *)a[1]);
      break;
    case SYS_read:
      c->GPRx = fs_read(a[1], (void *)a[2], a[3]);
      break;
    case SYS_write:
      c->GPRx = fs_write(a[1], (void *)a[2], a[3]);
      break;
    case SYS_lseek:
      c->GPRx = fs_lseek(a[1], a[2], a[3]);
      break;
    case SYS_close:
      c->GPRx = fs_close(a[1]);
      break;
    case SYS_brk:
      c->GPRx = mm_brk(a[1]);
      break;
    case SYS_gettimeofday:
      {
        struct timeval *tv = (struct timeval *)a[1];
        if (tv != NULL) {
          AM_TIMER_UPTIME_T uptime = io_read(AM_TIMER_UPTIME);
          tv->tv_sec = uptime.us / 1000000;
          tv->tv_usec = uptime.us % 1000000;
        }
        c->GPRx = 0;
        break;
      }
    default: panic("Unhandled syscall ID = %d", a[0]);
  }

#ifdef STRACE
  if (a[0] != SYS_write || a[3] > 1) {
    Log("[strace] %s return %p", syscall_names[a[0]], (void *)c->GPRx);
  }
#endif
}
