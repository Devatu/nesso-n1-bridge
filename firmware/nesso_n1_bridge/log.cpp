#include "log.h"
#include "log_ring.h"
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

  char body[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(body, sizeof(body), fmt, args);
  va_end(args);

  char line[192];
  snprintf(line, sizeof(line), "%s [%s] %s", name, tag ? tag : "?", body);
  log_ring_append(line);

#if defined(ARDUINO) && !defined(HOST_TEST)
  /* Geen Serial.flush(): dat blokkeert vaak USB-CDC en lijkt alsof er geen log is. */
  Serial.println(line);
#else
  printf("%s\n", line);
  fflush(stdout);
#endif
}
