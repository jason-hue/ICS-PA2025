#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/_default_fcntl.h>

static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;
static int canvas_x = 0, canvas_y = 0;
static int canvas_w = 0, canvas_h = 0;

uint32_t NDL_GetTicks() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

int NDL_PollEvent(char *buf, int len) {
  if (evtdev == -1) {
    evtdev = open("/dev/events", O_RDONLY);
  }
  int nread = read(evtdev, buf, len);
  if (nread > 0) {
    if (nread < len) {
      buf[nread] = '\0';
    }
    return 1;
  }
  return 0;
}

void NDL_OpenCanvas(int *w, int *h) {
  if (getenv("NWM_APP")) {
    int fbctl = 4;
    fbdev = 5;
    screen_w = *w; screen_h = *h;
    char buf[64];
    int len = sprintf(buf, "%d %d", screen_w, screen_h);
    // let NWM resize the window and create the frame buffer
    write(fbctl, buf, len);
    while (1) {
      // 3 = evtdev
      int nread = read(3, buf, sizeof(buf) - 1);
      if (nread <= 0) continue;
      buf[nread] = '\0';
      if (strcmp(buf, "mmap ok") == 0) break;
    }
    close(fbctl);
  }else
  {
    if (*w == 0 && *h == 0) {
      *w = screen_w;
      *h = screen_h;
    }
    if (*w > screen_w) *w = screen_w;
    if (*h > screen_h) *h = screen_h;
    canvas_w = *w; canvas_h = *h;
    // 计算居中偏移量
    canvas_x = (screen_w - canvas_w) / 2;
    canvas_y = (screen_h - canvas_h) / 2;
  }
}

void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
  int absolute_x = x + canvas_x;
  int absolute_y = y + canvas_y;
  if (fbdev == -1) {
    fbdev = open("/dev/fb", O_RDWR);
  }
  for (int i = 0; i < h; i++) {
    lseek(fbdev, ((absolute_y + i) * screen_w + absolute_x) * sizeof(uint32_t), SEEK_SET);
    write(fbdev, pixels + i * w, w * sizeof(uint32_t));
  }
}

void NDL_OpenAudio(int freq, int channels, int samples) {
}

void NDL_CloseAudio() {
}

int NDL_PlayAudio(void *buf, int len) {
  return 0;
}

int NDL_QueryAudio() {
  return 0;
}

int NDL_Init(uint32_t flags) {
  if (getenv("NWM_APP")) {
    evtdev = 3;
  }else
  {
    int fd = open("/proc/dispinfo", O_RDONLY);
    if (fd >= 0) {
      char buf[64];
      int n = read(fd, buf, sizeof(buf) - 1);
      if (n > 0) {
        buf[n] = '\0';
        sscanf(buf, "WIDTH : %d\nHEIGHT : %d", &screen_w, &screen_h);
      }
      close(fd);
    }
    evtdev = open("/dev/events", O_RDONLY);
    fbdev = open("/dev/fb", O_RDWR);
  }
  return 0;
}

void NDL_Quit() {
  close(evtdev);
  close(fbdev);
}
