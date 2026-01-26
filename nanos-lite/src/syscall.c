#include <common.h>
#include "syscall.h"

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
  Log("[strace] %s(%p, %p, %p)", syscall_names[a[0]], a[1], a[2], a[3]);
#endif

  switch (a[0]) {
    case SYS_exit:
      halt(a[1]);
      break;
    case SYS_yield:
      yield();
      c->GPRx = 0;// return 0
      break;
    default: panic("Unhandled syscall ID = %d", a[0]);
  }

#ifdef STRACE
  Log("[strace] %s return %p", syscall_names[a[0]], c->GPRx);
#endif
}
