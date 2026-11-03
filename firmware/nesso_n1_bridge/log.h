#ifndef NESSO_LOG_H
#define NESSO_LOG_H

#include "host_compat.h"

enum LogLevel { LOG_ERROR = 1, LOG_WARN = 2, LOG_INFO = 3, LOG_DEBUG = 4 };

void nesso_log(LogLevel level, const char *tag, const char *fmt, ...);

#define LOG_E(tag, fmt, ...) nesso_log(LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) nesso_log(LOG_WARN, tag, fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) nesso_log(LOG_INFO, tag, fmt, ##__VA_ARGS__)
#define LOG_D(tag, fmt, ...) nesso_log(LOG_DEBUG, tag, fmt, ##__VA_ARGS__)

#endif
