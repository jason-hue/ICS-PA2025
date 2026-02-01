#include <NDL.h>
#include <SDL.h>
#include <string.h>
#include <stdio.h>

#define keyname(k) #k,

static const char *keyname[] = {
  "NONE",
  _KEYS(keyname)
};

static uint8_t keystate[sizeof(keyname) / sizeof(keyname[0])];

int SDL_PushEvent(SDL_Event *ev) {
  return 0;
}

int SDL_PollEvent(SDL_Event *ev) {
  char buf[64];
  if (NDL_PollEvent(buf, sizeof(buf))) {
    char type[3], name[32];
    if (sscanf(buf, "%s %s", type, name) != 2) return 0;

    if (strcmp(type, "kd") == 0) ev->type = SDL_KEYDOWN;
    else if (strcmp(type, "ku") == 0) ev->type = SDL_KEYUP;
    else return 0;

    for (int i = 0; i < sizeof(keyname) / sizeof(keyname[0]); i++) {
      if (strcmp(name, keyname[i]) == 0) {
        ev->key.keysym.sym = i;
        ev->key.type = ev->type;
        keystate[i] = (ev->type == SDL_KEYDOWN);
        return 1;
      }
    }
  }
  return 0;
}

int SDL_WaitEvent(SDL_Event *event) {
  while (SDL_PollEvent(event) == 0);
  return 1;
}

int SDL_PeepEvents(SDL_Event *ev, int numevents, int action, uint32_t mask) {
  return 0;
}

uint8_t* SDL_GetKeyState(int *numkeys) {
  if (numkeys) *numkeys = sizeof(keystate) / sizeof(keystate[0]);
  return keystate;
}
