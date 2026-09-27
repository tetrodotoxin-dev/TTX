// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_PROTOCOL_DIRECT_PROVIDER_H
#define TTX_DATA_PROTOCOL_DIRECT_PROVIDER_H

#include "ttx/data/form/representation.h"

// Direct lends an existing payload pointer under its publication lifetime.
// The provider retains the representation and payload while consumers use
// them. Agreement permits interpreting that public pointer by its descriptor,
// while the receiver behind each operation remains private provider state.

typedef struct ttx_direct_provider_operations {
  const ttx_representation* (*representation)(const void* source);
  const void* (*read_ptr)(const void* source);
} ttx_direct_provider_operations;

typedef struct ttx_direct_provider {
  const void* source;
  const ttx_direct_provider_operations* operations;
} ttx_direct_provider;

// Describe the callable API independently of the payload it transports.
PERIMORTEM_C const ttx_representation* ttx_direct_provider_representation(void);

#endif
