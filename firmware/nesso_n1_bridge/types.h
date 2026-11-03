#ifndef NESSO_TYPES_H
#define NESSO_TYPES_H

#include "app_config.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct Eui64 {
  uint8_t bytes[8];
};

inline void eui64_clear(Eui64 *eui) {
  memset(eui->bytes, 0, sizeof(eui->bytes));
}

inline bool eui64_is_zero(const Eui64 *eui) {
  for (int i = 0; i < 8; ++i) {
    if (eui->bytes[i] != 0) {
      return false;
    }
  }
  return true;
}

inline bool eui64_equal(const Eui64 *a, const Eui64 *b) {
  return memcmp(a->bytes, b->bytes, 8) == 0;
}

inline void eui64_copy(Eui64 *dst, const Eui64 *src) {
  memcpy(dst->bytes, src->bytes, 8);
}

struct ButtonEvent {
  Eui64 source;
  uint8_t button; /* 1..4 */
  ButtonKind kind;
  uint32_t timestamp_ms;
  bool source_ieee_resolved;
};

struct ZclIncoming {
  uint16_t src_short;
  Eui64 src_ieee;
  bool ieee_resolved;
  uint8_t src_endpoint;
  uint16_t cluster;
  uint8_t command;
  uint8_t scene_id;
  uint8_t step_mode;
};

#endif
