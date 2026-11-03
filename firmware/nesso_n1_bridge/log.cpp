#include "log.h"
#include <stdarg.h>
#include <stdio.h>

#ifndef NESSO_LOG_LEVEL
#define NESSO_LOG_LEVEL LOG_INFO
#endif

void nesso_log(LogLevel level, const char *tag, const char *fmt, ...) {
  if (level > NESSO_LOG_LEVEL) {
    return;
  }
  const char *name = "I";
  if (level == LOG_ERROR) {
    name = "E";
  } else if (level == LOG_WARN) {
    name = "W";
  } else if (level == LOG_DEBUG) {
    name = "D";
  }
  printf("%s [%s] ", name, tag);
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf("\n");
}
