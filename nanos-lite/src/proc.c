#include <proc.h>

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

void switch_boot_pcb() {
  current = &pcb_boot;
}

void hello_fun(void *arg) {
  int j = 1;
  while (1) {
    Log("Hello World from Nanos-lite with arg '%p' for the %dth time!", (uintptr_t)arg, j);
    j ++;
    yield();
  }
}

void init_proc() {
  switch_boot_pcb();

  Log("Initializing processes...");

  // load program here
  // naive_uload(NULL, "/bin/bmp-test");
  context_kload(&pcb[0], hello_fun, "A");
  context_kload(&pcb[1], hello_fun, "B");

}
static int ptr = MAX_NR_PROC - 1;
Context* schedule(Context *prev) {
  current->cp = prev;
  for (int i = 0; i < MAX_NR_PROC; i++) {
    ptr = (ptr + 1) % MAX_NR_PROC;
    if (pcb[ptr].cp != NULL) {
      current = &pcb[ptr];
      return current->cp;
    }
  }
  panic("No runnable process!");
  return NULL;
}
void context_kload(PCB *pcb, void (*entry)(void *), void *arg) {
  pcb->cp = kcontext((Area){pcb->stack, pcb->stack + STACK_SIZE}, entry, arg);
}