// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_PROTOCOL_BLOCK_PROVIDER_H
#define TTX_DATA_PROTOCOL_BLOCK_PROVIDER_H

#include "ttx/data/form/storage.h"

// Block produces a complete record into the caller's admitted Storage without
// lending provider storage. Its representation must match the agreed payload.
// The provider writes only that representation's extent, leaving spare capacity
// untouched. Each `commit` finishes before returning and retains no destination
// borrow. Failure supplies no readable result even if some bytes were written.
// A policy that needs deferred work owns that execution outside this contract.

typedef struct ttx_block_provider_operations {
  const ttx_representation* (*representation)(void* source);
  ttx_data_status (*commit)(void* source, ttx_storage target);
} ttx_block_provider_operations;

typedef struct ttx_block_provider {
  void* source;
  const ttx_block_provider_operations* operations;
} ttx_block_provider;

// Describe the callable API independently of the payload it transports.
PERIMORTEM_C const ttx_representation* ttx_block_provider_representation(void);

#endif
