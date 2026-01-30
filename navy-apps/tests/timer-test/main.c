#include <stdio.h>
#include <sys/time.h>
#include <stdint.h>

int main() {
  struct timeval tv;
  while (1) {
    gettimeofday(&tv, NULL);
    uint64_t ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    while (1) {
      gettimeofday(&tv, NULL);
      uint64_t ms_now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
      if (ms_now - ms >= 500) break;
    }
    printf("0.5 seconds passed\n");
  }
  return 0;
}
