// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_REALIZATION_INVOCATION_H
#define TTX_SEMANTIC_REALIZATION_INVOCATION_H

#include "ttx/semantic/negotiation/query.h"

// Invocation calls an operation through represented input and output buffers.
// A consumer uses it when the payload layouts are known but the provider's
// native function declaration is private. Negotiating the operation's UUID
// and this API record establishes the call convention. The input and output
// Representations describe the buffers passed to `invoke`.
//
// The caller supplies accessible, correctly sized and aligned buffers matching
// those Representations. The provider borrows them until the synchronous call
// returns. The consumer uses output after TTX_DATA_SUCCESS. The operation's
// contract defines any other effects and any aliasing it permits.
//
// The supplying contract keeps context, Representations and executable code
// accessible through each call. Copying the API record preserves that access
// duration. Independent retention and release are supplied by the higher
// contract governing the operation.
typedef struct ttx_invocation {
  void* context;
  const ttx_representation* inputs;
  const ttx_representation* outputs;
  ttx_data_status (*invoke)(void* context, const void* inputs, void* outputs);
} ttx_invocation;

C_LINKAGE EXPORTED(TTX) const ttx_representation* ttx_invocation_representation(
    void);

#endif
