#ifndef NESSO_HOST_COMPAT_H
#define NESSO_HOST_COMPAT_H

#ifdef HOST_TEST
#include <stdarg.h>
#include <stdio.h>

struct HostSerial {
  void begin(unsigned long) {}
  void println(const char *s) { printf("%s\n", s); }
  void print(const char *s) { fputs(s, stdout); }
  void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
  }
};

static HostSerial Serial __attribute__((unused));

inline unsigned long millis() {
  return 0;
}

#else
#include <Arduino.h>
#endif

#endif
