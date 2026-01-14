#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  size_t len = 0;
  while (*s != '\0')
  {
    len++;
    s++;
  }
  return len;
}

char *strcpy(char *dst, const char *src) {
  char *ret = dst;//dst指针会增加，ret保存起点指针
  while ((*dst++ = *src++) != '\0') ;
  return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
  size_t i;
  for (i = 0; i < n && src[i] != '\0'; i++) {
    dst[i] = src[i];
  }
  // 如果 src 长度小于 n，剩余部分必须填满 '\0'
  for (; i < n; i++) {
    dst[i] = '\0';
  }
  return dst;
}

char *strcat(char *dst, const char *src) {
  char *ret = dst;
  while (*dst != '\0') {
    dst++;
  }//dst找到结尾了
  while ((*dst++ = *src++) != '\0') ;//从dst结尾拼接src
  return ret;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 != '\0' && *s1 == *s2) {
    s1++;
    s2++;
  }
  return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (s1[i] != s2[i] || s1[i] == '\0') {
      return (unsigned char)s1[i] - (unsigned char)s2[i];
    }
  }
  return 0;
}

void *memset(void *s, int c, size_t n) {
  char *p = (char *)s;
  while (n-- > 0) {
    *p++ = c;//先赋值再++
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;

  if (s < d && d < s + n) {
    // 情况：目标地址在源地址后面且有重叠，从后往前拷贝
    for (size_t i = n; i > 0; i--) {
      d[i - 1] = s[i - 1];
    }
  } else {
    // 情况：没有重叠，或者目标地址在源地址前面，从前往后拷贝
    for (size_t i = 0; i < n; i++) {
      d[i] = s[i];
    }
  }

  return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
  unsigned char *dst = (unsigned char *)out;
  const unsigned char *src = (const unsigned char *)in;

  for (size_t i = 0; i < n; i++) {
    dst[i] = src[i];
  }

  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;

  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return (int)p1[i] - (int)p2[i];
    }
  }

  return 0;
}

#endif
