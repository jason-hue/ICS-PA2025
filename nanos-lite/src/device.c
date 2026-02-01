#include <common.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
# define MULTIPROGRAM_YIELD() yield()
#else
# define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) \
  [AM_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [AM_KEY_NONE] = "NONE",
  AM_KEYS(NAME)
};

size_t serial_write(const void *buf, size_t offset, size_t len) {
  for (size_t i = 0; i < len; i++) {
    putch(((char *)buf)[i]);
  }
  return len;
}

size_t events_read(void *buf, size_t offset, size_t len) {
  AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
  if (ev.keycode == AM_KEY_NONE) {
    return 0;
  }
  char temp[64];
  int n = snprintf(temp, sizeof(temp), "%s %s\n",
                   ev.keydown ? "kd" : "ku",
                   keyname[ev.keycode]);
  size_t act_len = (n < len) ? n : len;
  memcpy(buf, temp, act_len);
  return act_len;
}

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  char dispinfo[128];
  int n = snprintf(dispinfo, sizeof(dispinfo), "WIDTH : %d\nHEIGHT : %d\n", cfg.width, cfg.height);
  if (offset >= n) return 0;
  size_t act_len = (n - offset < len) ? (n - offset) : len;
  memcpy(buf, dispinfo + offset, act_len);
  return act_len;
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  int screen_w = cfg.width;
  int pixel_offset = offset / sizeof(uint32_t);
  int x = pixel_offset % screen_w;
  int y = pixel_offset / screen_w;
  io_write(AM_GPU_FBDRAW, x, y, (uint32_t *)buf, len / sizeof(uint32_t), 1, true);
  return len;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
