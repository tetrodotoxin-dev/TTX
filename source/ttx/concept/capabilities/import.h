// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_CAPABILITIES_IMPORT_H
#define TTX_CONCEPT_CAPABILITIES_IMPORT_H
#include "ttx/concept/abstract.h"

#define TTX_IMPORT_ID_HIGH 0x73d6db5cd5954412ULL
#define TTX_IMPORT_ID_LOW 0xb8020612f7ea0361ULL

// Exposes the capability to interpret data described by the supplied
// Representation and supply an Abstract for the imported graph. The provider
// checks the representation before interpreting the input. On success, `visit`
// calls `callback` once with that Abstract and returns Satisfied after the
// callback finishes.
//
// The caller can supply callback state through `callback_context`, which is
// passed unchanged to `callback`. The input, Representation and callback state
// remain available until `visit` returns. The imported graph is available
// during the callback, where the consumer can inspect it, copy data or
// negotiate retained access through the graph's capabilities.
//
// A `visit` returning Unknown or Rejected guarantees that `callback` was never
// called. Unknown leaves the import request undetermined. Rejected explicitly
// refuses it. The provider supplies `visit` and chooses the returned graph.
typedef struct ttx_import {
  void* context;
  const struct ttx_import_ops* operations;
} ttx_import;

typedef struct ttx_import_ops {
  ttx_abstract_ops abstract;
  ttx_binding_status (*visit)(
      void* context,
      const void* input,
      const ttx_representation* representation,
      void* callback_context,
      void (*callback)(void* context, ttx_abstract abstract));
} ttx_import_ops;

C_LINKAGE const ttx_representation* ttx_import_representation(void);
#endif
