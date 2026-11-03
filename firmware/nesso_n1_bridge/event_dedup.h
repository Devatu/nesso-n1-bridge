#ifndef NESSO_EVENT_DEDUP_H
#define NESSO_EVENT_DEDUP_H

#include "types.h"

struct DedupRecord {
  Eui64 eui;
  uint8_t button;
  ButtonKind kind;
  uint32_t last_ms;
  bool occupied;
};

struct EventDedup {
  DedupRecord records[NESSO_MAX_SHELLY_SLOTS * 4];
  uint32_t window_ms;
};

void dedup_init(EventDedup *dedup, uint32_t window_ms);
bool dedup_accept(EventDedup *dedup, const ButtonEvent *event);

#endif
