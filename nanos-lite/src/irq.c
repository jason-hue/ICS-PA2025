#include <common.h>
#include <unistd.h>
#include <proc.h>

static Context* do_event(Event e, Context* c) {
  switch (e.event) {
    case EVENT_YIELD: {
      // printf("do_event: EVENT_YIELD\n");
      c = schedule(c);
      break;
    }
    case EVENT_IRQ_TIMER:
      // Log("Timer interrupt");
      c = schedule(c);
      break;
    case EVENT_SYSCALL:
      {
        do_syscall(c);
        break;
      }
    default: panic("Unhandled event ID = %d", e.event);
  }

  return c;
}

void init_irq(void) {
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}
