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
  static char *busybox_argv[] = {"/bin/busybox", "echo", "hello", "navy", NULL};
  static char *empty_envp[] = {NULL};
  context_uload(&pcb[0], "/bin/busybox", busybox_argv, empty_envp);
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

static uintptr_t setup_stack(void * stack_top, char * const * argv, char * const * envp) {
  int argc = 0;
  if (argv) { while (argv[argc]) argc++; }
  int envc = 0;
  if (envp) { while (envp[envc]) envc++; }
  char *stack_ptr = (char *)stack_top;
  char *argv_ptrs[argc];
  char *envp_ptrs[envc];
  for (int i = 0; i < envc; i++) {
    stack_ptr -= (strlen(envp[i]) + 1);
    strcpy(stack_ptr, envp[i]);
    envp_ptrs[i] = stack_ptr;
  }
  for (int i = 0; i < argc; i++) {
    stack_ptr -= (strlen(argv[i]) + 1);
    strcpy(stack_ptr, argv[i]);
    argv_ptrs[i] = stack_ptr;
  }
  uintptr_t *table_ptr = (uintptr_t *)((uintptr_t)stack_ptr & ~0x3);
  table_ptr--; *table_ptr = 0;
  for (int i = envc - 1; i >= 0; i--) {
    table_ptr--; *table_ptr = (uintptr_t)envp_ptrs[i];
  }
  table_ptr--; *table_ptr = 0;
  for (int i = argc - 1; i >= 0; i--) {
    table_ptr--; *table_ptr = (uintptr_t)argv_ptrs[i];
  }
  table_ptr--; *table_ptr = (uintptr_t)argc;
  return (uintptr_t)table_ptr;
}

void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]) {
  void *stack_origin = new_page(8);
  void *stack_top = stack_origin + 32 * 1024;
  uintptr_t arg_addr = setup_stack(stack_top, argv, envp);
  Area kstack = {pcb->stack, pcb->stack + STACK_SIZE};
  void *entry = (void *)loader(pcb, filename);
  pcb->cp = ucontext(NULL, kstack, entry);
  pcb->cp->eax = (uintptr_t)arg_addr;
}
