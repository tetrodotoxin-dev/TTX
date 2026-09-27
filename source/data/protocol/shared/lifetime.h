// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_PROTOCOL_SHARED_LIFETIME_H
#define TTX_DATA_PROTOCOL_SHARED_LIFETIME_H

#include "ttx/data/form/representation.h"

// A Shared lifetime separates the borrowed payload from its release owner.
// The data pointer follows the agreed representation. The opaque source is
// used only by release, allowing an interior payload to retain its real owner.

typedef struct ttx_shared_lifetime {
  const void* data;
  const void* source;
  void (*release)(const void* source);
} ttx_shared_lifetime;

// Clear the obligation before calling release so reentry cannot release twice.
PERIMORTEM_C void ttx_shared_release(ttx_shared_lifetime* lifetime);

#endif
