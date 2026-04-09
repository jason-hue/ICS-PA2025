/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <common.h>
#include <device/map.h>
#include <SDL2/SDL.h>

enum {
  reg_freq,
  reg_channels,
  reg_samples,
  reg_sbuf_size,
  reg_init,
  reg_count,
  nr_reg
};

static uint8_t *sbuf = NULL;
static uint32_t *audio_base = NULL;
static SDL_AudioSpec audio_spec = {};
static bool audio_inited = false;
static uint32_t sbuf_rpos = 0;
static uint32_t sbuf_wpos = 0;

static inline uint32_t audio_queue_count(void) {
  return audio_base[reg_count];
}

static inline uint32_t audio_sbuf_size(void) {
  return audio_base[reg_sbuf_size];
}

static void audio_callback(void *userdata, uint8_t *stream, int len) {
  uint32_t count = audio_queue_count();
  uint32_t nread = len;
  uint32_t first;
  (void)userdata;

  if (count < nread) {
    nread = count;
  }

  if (nread) {
    first = nread;
    if (sbuf_rpos + first > audio_sbuf_size()) {
      first = audio_sbuf_size() - sbuf_rpos;
    }
    memcpy(stream, sbuf + sbuf_rpos, first);
    sbuf_rpos = (sbuf_rpos + first) % audio_sbuf_size();
    if (nread > first) {
      memcpy(stream + first, sbuf + sbuf_rpos, nread - first);
      sbuf_rpos = (sbuf_rpos + (nread - first)) % audio_sbuf_size();
    }
    audio_base[reg_count] -= nread;
  }

  if ((uint32_t) len > nread) {
    memset(stream + nread, 0, len - nread);
  }
}

static void audio_init_backend(void) {
  if (audio_inited) {
    SDL_CloseAudio();
    audio_inited = false;
  }

  audio_spec.freq = audio_base[reg_freq];
  audio_spec.channels = audio_base[reg_channels];
  audio_spec.samples = audio_base[reg_samples];
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.callback = audio_callback;
  audio_spec.userdata = NULL;

  sbuf_rpos = 0;
  sbuf_wpos = 0;
  audio_base[reg_count] = 0;

  if (audio_spec.freq && audio_spec.channels && audio_spec.samples) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0 && SDL_OpenAudio(&audio_spec, NULL) == 0) {
      SDL_PauseAudio(0);
      audio_inited = true;
    }
  }
}

static void audio_io_handler(uint32_t offset, int len, bool is_write) {
  if (!is_write && offset == reg_sbuf_size * sizeof(uint32_t)) {
    audio_base[reg_sbuf_size] = CONFIG_SB_SIZE;
    return;
  }

  if (!is_write && offset == reg_count * sizeof(uint32_t)) {
    return;
  }

  if (!is_write) {
    return;
  }

  if (offset == reg_init * sizeof(uint32_t) && audio_base[reg_init]) {
    audio_init_backend();
    audio_base[reg_init] = 0;
    return;
  }

  if (offset == reg_count * sizeof(uint32_t)) {
    uint32_t appended = audio_base[reg_count];
    uint32_t queued = audio_queue_count();
    uint32_t free = audio_sbuf_size() - queued;
    if (appended > free) {
      appended = free;
    }
    sbuf_wpos = (sbuf_wpos + appended) % audio_sbuf_size();
    audio_base[reg_count] = queued + appended;
  }
}

void init_audio() {
  uint32_t space_size = sizeof(uint32_t) * nr_reg;
  audio_base = (uint32_t *)new_space(space_size);
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("audio", CONFIG_AUDIO_CTL_PORT, audio_base, space_size, audio_io_handler);
#else
  add_mmio_map("audio", CONFIG_AUDIO_CTL_MMIO, audio_base, space_size, audio_io_handler);
#endif

  sbuf = (uint8_t *)new_space(CONFIG_SB_SIZE);
  add_mmio_map("audio-sbuf", CONFIG_SB_ADDR, sbuf, CONFIG_SB_SIZE, NULL);
  audio_base[reg_sbuf_size] = CONFIG_SB_SIZE;
  audio_base[reg_count] = 0;
}
