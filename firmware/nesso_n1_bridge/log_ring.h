#ifndef NESSO_LOG_RING_H
#define NESSO_LOG_RING_H

#include <stddef.h>

void log_ring_append(const char *line);
/* Schrijf chronologische snapshot naar out (NUL-terminated). */
size_t log_ring_copy(char *out, size_t out_len);

#endif
