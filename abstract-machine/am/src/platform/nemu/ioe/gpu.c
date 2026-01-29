#include <am.h>
#include <nemu.h>

#define SYNC_ADDR (VGACTL_ADDR + 4)

void __am_gpu_init() {
}

void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  uint32_t info = inl(VGACTL_ADDR);
  uint32_t w = info >> 16;
  uint32_t h = info & 0xffff;
  *cfg = (AM_GPU_CONFIG_T) {
    .present = true, .has_accel = false,
    .width = w, .height = h,
    .vmemsz = w * h * sizeof(uint32_t)
  };
}

void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  int SCREEN_W = io_read(AM_GPU_CONFIG).width;
  int SCREEN_H = io_read(AM_GPU_CONFIG).height;

  uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR;
  uint32_t *px = (uint32_t *)ctl->pixels;

  // 逐行拷贝，比逐像素拷贝更快
  for (int i = 0; i < ctl->h; i++) {
    if (ctl->y + i >= SCREEN_H) break;

    for (int j = 0; j < ctl->w; j++) {
      if (ctl->x + j >= SCREEN_W) break;
      fb[(ctl->y + i) * SCREEN_W + (ctl->x + j)] = px[i * ctl->w + j];
    }
  }
  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
}

void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = true;
}
