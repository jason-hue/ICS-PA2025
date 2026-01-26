#include <common.h>
#include "syscall.h"
#include <memory.h>

#ifdef STRACE
static const char *syscall_names[] = {
  "exit", "yield", "open", "read", "write", "kill", "getpid", "close",
  "lseek", "brk", "fstat", "time", "signal", "execve", "fork", "link",
  "unlink", "wait", "times", "gettimeofday"
};
#endif

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1; // 系统调用号
  a[1] = c->GPR2; // 参数 1
  a[2] = c->GPR3; // 参数 2
  a[3] = c->GPR4; // 参数 3

#ifdef STRACE
  // 仅在非 write 或 write 长度大于 1 时打印，减少噪音
  if (a[0] != SYS_write || a[3] > 1) {
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
    case SYS_write:
      if (a[1] == 1 || a[1] == 2) {
        char *buf = (char *)a[2];
        for (int i = 0; i < a[3]; i++) {
          putch(buf[i]);
        }
        c->GPRx = a[3];
      } else {
        c->GPRx = -1;
      }
      break;
    case SYS_brk:
      c->GPRx = mm_brk(a[1]);
      break;
    default: panic("Unhandled syscall ID = %d", a[0]);
  }

#ifdef STRACE
  if (a[0] != SYS_write || a[3] > 1) {
    Log("[strace] %s return %p", syscall_names[a[0]], (void *)c->GPRx);
  }
#endif
}
