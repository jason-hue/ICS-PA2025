#ifndef ARCH_H__
#define ARCH_H__

#include <stdint.h>

struct Context {
  uintptr_t esp;
  void *cr3;
  uintptr_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
  int irq;
  uintptr_t eip, cs, eflags;
};

#define GPR1 eax
#define GPR2 ebx
#define GPR3 ecx
#define GPR4 edx
#define GPRx eax

#endif
