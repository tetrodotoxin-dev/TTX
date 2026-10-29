// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_CAPABILITIES_IMPORT_H
#define TTX_GRAPH_CAPABILITIES_IMPORT_H

#include "ttx/graph/abstract.h"

#define TTX_IMPORT_ID_HIGH 0x2793e7c714e64355ULL
#define TTX_IMPORT_ID_LOW 0xb6104e50092bf2e2ULL

// Exposes the capability to interpret data described by the supplied
// Representation and supply an Abstract for the imported graph. The provider
// checks the representation before interpreting the input. On success, `visit`
// calls `callback` once with that Abstract and returns Satisfied after the
// callback finishes.
//
// The caller can supply callback state through `callback_context`, which is
// passed unchanged to `callback`. `visit` observes the input, Representation
// and callback state within one synchronous transaction and transfers no
// access to them beyond return. Input storage satisfies the extent and
// alignment of its canonical Representation. The imported graph is available
// during the callback, where the consumer can inspect it or copy the typed data
// it needs after the observation.
//
// A `visit` returning Unknown or Rejected guarantees that `callback` was never
// called. Unknown leaves the import request undetermined. Rejected explicitly
// refuses it. The provider supplies `visit` and chooses the returned graph.
typedef struct ttx_import {
  void* context;
  const struct ttx_import_ops* operations;
} ttx_import;

typedef struct ttx_import_ops {
  ttx_negotiation_status (*visit)(
      void* context,
      const void* input,
      const ttx_representation* representation,
      void* callback_context,
      void (*callback)(void* callback_context, ttx_abstract abstract));
} ttx_import_ops;

#endif
