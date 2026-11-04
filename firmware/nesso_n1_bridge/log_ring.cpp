#include "log_ring.h"
#include <stdio.h>
#include <string.h>

/* Groter tijdens knop-debug — portal /api/logs toont meer history. */
static const size_t kLines = 120;
static const size_t kLineLen = 180;
static char g_lines[kLines][kLineLen];
static size_t g_head = 0;
static size_t g_count = 0;

void log_ring_append(const char *line) {
  if (line == nullptr) {
    return;
  }
  snprintf(g_lines[g_head], kLineLen, "%s", line);
  g_head = (g_head + 1) % kLines;
  if (g_count < kLines) {
    g_count++;
  }
}

size_t log_ring_copy(char *out, size_t out_len) {
  if (out == nullptr || out_len == 0) {
    return 0;
  }
  out[0] = '\0';
  size_t used = 0;
  const size_t start = (g_count < kLines) ? 0 : g_head;
  for (size_t i = 0; i < g_count; ++i) {
    const size_t idx = (start + i) % kLines;
    const int n = snprintf(out + used, out_len - used, "%s\n", g_lines[idx]);
    if (n < 0 || (size_t)n >= out_len - used) {
      out[out_len - 1] = '\0';
      return out_len - 1;
    }
    used += (size_t)n;
  }
  return used;
}
