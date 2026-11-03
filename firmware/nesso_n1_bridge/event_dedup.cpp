#include "event_dedup.h"
#include <string.h>

void dedup_init(EventDedup *dedup, uint32_t window_ms) {
  memset(dedup, 0, sizeof(*dedup));
  dedup->window_ms = window_ms;
}

bool dedup_accept(EventDedup *dedup, const ButtonEvent *event) {
  if (dedup == nullptr || event == nullptr) {
    return false;
  }
  const int n = (int)(sizeof(dedup->records) / sizeof(dedup->records[0]));
  int free_idx = -1;
  for (int i = 0; i < n; ++i) {
    DedupRecord *r = &dedup->records[i];
    if (!r->occupied) {
      if (free_idx < 0) {
        free_idx = i;
      }
      continue;
    }
    if (eui64_equal(&r->eui, &event->source) && r->button == event->button && r->kind == event->kind) {
      if (event->timestamp_ms >= r->last_ms && (event->timestamp_ms - r->last_ms) < dedup->window_ms) {
        return false;
      }
      r->last_ms = event->timestamp_ms;
      return true;
    }
  }
  if (free_idx < 0) {
    return false;
  }
  DedupRecord *r = &dedup->records[free_idx];
  eui64_copy(&r->eui, &event->source);
  r->button = event->button;
  r->kind = event->kind;
  r->last_ms = event->timestamp_ms;
  r->occupied = true;
  return true;
}
