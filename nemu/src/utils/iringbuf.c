#include <common.h>

#ifdef CONFIG_IRINGBUF

#define RINGBUF_SIZE 16

typedef struct {
  char log[128];
} IRingBufItem;

static IRingBufItem ringbuf[RINGBUF_SIZE];
static int ringbuf_end = 0; // Points to the next write position

void iringbuf_write(const char *log) {
  if (!log) return;
  strncpy(ringbuf[ringbuf_end].log, log, 128);
  ringbuf[ringbuf_end].log[127] = '\0'; // Ensure null-termination
  ringbuf_end = (ringbuf_end + 1) % RINGBUF_SIZE;
}

void iringbuf_display() {
  if (!ringbuf[0].log[0]) return;

  printf("Most recent instructions:\n");
  int i = ringbuf_end;
  do {
    if (ringbuf[i].log[0] != '\0') {
      printf("  %s\n", ringbuf[i].log);
    }
    i = (i + 1) % RINGBUF_SIZE;
  } while (i != ringbuf_end);
}

#else

void iringbuf_write(const char *log) {}
void iringbuf_display() {}

#endif
