#include <am.h>
#include <klib.h>
#include <nemu.h>

#define AUDIO_FREQ_ADDR      (AUDIO_ADDR + 0x00)
#define AUDIO_CHANNELS_ADDR  (AUDIO_ADDR + 0x04)
#define AUDIO_SAMPLES_ADDR   (AUDIO_ADDR + 0x08)
#define AUDIO_SBUF_SIZE_ADDR (AUDIO_ADDR + 0x0c)
#define AUDIO_INIT_ADDR      (AUDIO_ADDR + 0x10)
#define AUDIO_COUNT_ADDR     (AUDIO_ADDR + 0x14)

static uint32_t audio_sbuf_size;
static uint32_t audio_wpos;

void __am_audio_init() {
  audio_sbuf_size = inl(AUDIO_SBUF_SIZE_ADDR);
  audio_wpos = 0;
}

void __am_audio_config(AM_AUDIO_CONFIG_T *cfg) {
  cfg->present = audio_sbuf_size > 0;
  cfg->bufsize = audio_sbuf_size;
}

void __am_audio_ctrl(AM_AUDIO_CTRL_T *ctrl) {
  outl(AUDIO_FREQ_ADDR, ctrl->freq);
  outl(AUDIO_CHANNELS_ADDR, ctrl->channels);
  outl(AUDIO_SAMPLES_ADDR, ctrl->samples);
  outl(AUDIO_INIT_ADDR, 1);
  audio_wpos = 0;
}

void __am_audio_status(AM_AUDIO_STATUS_T *stat) {
  stat->count = inl(AUDIO_COUNT_ADDR);
}

void __am_audio_play(AM_AUDIO_PLAY_T *ctl) {
  uint8_t *src = ctl->buf.start;
  uint32_t len = (uintptr_t)ctl->buf.end - (uintptr_t)ctl->buf.start;

  while (len > 0) {
    uint32_t count = inl(AUDIO_COUNT_ADDR);
    uint32_t free = audio_sbuf_size - count;
    uint32_t chunk = len;
    uint32_t first;
    if (free == 0) {
      continue;
    }
    if (chunk > free) {
      chunk = free;
    }

    first = chunk;
    if (audio_wpos + first > audio_sbuf_size) {
      first = audio_sbuf_size - audio_wpos;
    }
    memcpy((void *)(uintptr_t)(AUDIO_SBUF_ADDR + audio_wpos), src, first);
    audio_wpos = (audio_wpos + first) % audio_sbuf_size;
    if (chunk > first) {
      memcpy((void *)(uintptr_t)(AUDIO_SBUF_ADDR + audio_wpos), src + first, chunk - first);
      audio_wpos = (audio_wpos + (chunk - first)) % audio_sbuf_size;
    }
    outl(AUDIO_COUNT_ADDR, chunk);
    src += chunk;
    len -= chunk;
  }
}
