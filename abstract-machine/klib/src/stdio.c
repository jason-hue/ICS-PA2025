#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

static void out_char(char *out, size_t n, size_t *pos, char ch) {
  if (n > 0 && *pos + 1 < n) {
    out[*pos] = ch;
  }
  (*pos)++;
}

static void out_str(char *out, size_t n, size_t *pos, const char *s) {
  if (s == NULL) {
    s = "(null)";
  }
  for (; *s; s++) {
    out_char(out, n, pos, *s);
  }
}

static void out_uint(char *out, size_t n, size_t *pos, uint32_t value, unsigned base, bool uppercase) {
  char buf[32];
  size_t len = 0;
  const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

  if (base < 2 || base > 16) {
    return;
  }

  do {
    buf[len++] = digits[value % base];
    value /= base;
  } while (value != 0 && len < sizeof(buf));

  while (len > 0) {
    out_char(out, n, pos, buf[--len]);
  }
}

static void out_int(char *out, size_t n, size_t *pos, int32_t value) {
  if (value < 0) {
    out_char(out, n, pos, '-');
    out_uint(out, n, pos, (uint32_t)(-value), 10, false);
  } else {
    out_uint(out, n, pos, (uint32_t)value, 10, false);
  }
}

static int format_output(char *out, size_t n, const char *fmt, va_list ap) {
  size_t pos = 0;

  for (const char *p = fmt; *p != '\0'; p++) {
    if (*p != '%') {
      out_char(out, n, &pos, *p);
      continue;
    }

    p++;
    if (*p == '\0') {
      break;
    }

    switch (*p) {
      case 's':
        out_str(out, n, &pos, va_arg(ap, const char *));
        break;
      case 'c':
        out_char(out, n, &pos, (char)va_arg(ap, int));
        break;
      case 'd':
        out_int(out, n, &pos, (int32_t)va_arg(ap, int));
        break;
      case 'u':
        out_uint(out, n, &pos, (uint32_t)va_arg(ap, unsigned int), 10, false);
        break;
      case 'x':
        out_uint(out, n, &pos, (uint32_t)va_arg(ap, unsigned int), 16, false);
        break;
      case 'p':
        out_str(out, n, &pos, "0x");
        out_uint(out, n, &pos, (uint32_t)(uintptr_t)va_arg(ap, void *), 16, false);
        break;
      case '%':
        out_char(out, n, &pos, '%');
        break;
      default:
        out_char(out, n, &pos, '%');
        out_char(out, n, &pos, *p);
        break;
    }
  }

  if (n > 0) {
    out[pos < n ? pos : n - 1] = '\0';
  }
  return (int)pos;
}

int printf(const char *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  int len = format_output(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  for (int i = 0; i < len && buf[i] != '\0'; i++) {
    putch(buf[i]);
  }
  return len;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  return format_output(out, (size_t)-1, fmt, ap);
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = format_output(out, (size_t)-1, fmt, ap);
  va_end(ap);
  return len;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = format_output(out, n, fmt, ap);
  va_end(ap);
  return len;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  return format_output(out, n, fmt, ap);
}

#endif
