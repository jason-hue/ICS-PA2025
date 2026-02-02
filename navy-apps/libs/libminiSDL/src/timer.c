#include <NDL.h>
#include <sdl-timer.h>
#include <stdio.h>

SDL_TimerID SDL_AddTimer(uint32_t interval, SDL_NewTimerCallback callback, void *param) {
  return NULL;
}

int SDL_RemoveTimer(SDL_TimerID id) {
  return 1;
}

uint32_t SDL_GetTicks() {
  static uint32_t start = 0;
  static int is_init = 0;
  if (!is_init) {
    start = NDL_GetTicks();
    is_init = 1;
  }
  return NDL_GetTicks() - start;
}

void SDL_Delay(uint32_t ms) {
  uint32_t start = NDL_GetTicks();
  while (NDL_GetTicks() - start < ms) ;
}
