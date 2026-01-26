#include <memory.h>
#include <proc.h>

static void *pf = NULL;

void* new_page(size_t nr_page) {
  void *old_pf = pf;
  pf += nr_page * PGSIZE;
  return old_pf;
}

#ifdef HAS_VME
static void* pg_alloc(int n) {
  void *ret = new_page(n / PGSIZE);
  memset(ret, 0, n);
  return ret;
}
#endif

void free_page(void *p) {
  panic("not implement yet");
}

/* The brk() system call handler. */
int mm_brk(uintptr_t brk) {
  current->max_brk = (brk > current->max_brk ? brk : current->max_brk);
#ifdef HAS_VME
  // PA4 阶段需要在这里进行页面分配和映射
#endif
  return 0;
}

void init_mm() {
  pf = (void *)ROUNDUP(heap.start, PGSIZE);
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}
