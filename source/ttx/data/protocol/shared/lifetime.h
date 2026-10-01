// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_PROTOCOL_SHARED_LIFETIME_H
#define TTX_DATA_PROTOCOL_SHARED_LIFETIME_H

#include "ttx/data/form/representation.h"

// A Shared lifetime separates the borrowed payload from the provider state
// used for release. The `data` pointer follows the agreed representation.
// `release` receives the opaque `source` pointer, so the provider can retain
// an allocation while lending a pointer to data within it.

typedef struct ttx_shared_lifetime {
  const void* data;
  void* source;
  void (*release)(void* source);
} ttx_shared_lifetime;

// Clear the obligation before calling `release` so reentry cannot release
// twice.
C_LINKAGE void ttx_shared_release(ttx_shared_lifetime* lifetime);

#endif
