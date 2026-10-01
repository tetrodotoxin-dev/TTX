// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_CAPABILITIES_IMPORT_H
#define TTX_CONCEPT_CAPABILITIES_IMPORT_H
#include "ttx/concept/abstract.h"

#define TTX_IMPORT_ID_HIGH 0x73d6db5cd5954412ULL
#define TTX_IMPORT_ID_LOW 0xb8020612f7ea0361ULL

// Exposes the capability to interpret data described by the supplied
// Representation and produce an Abstract projection. The provider checks the
// representation before interpreting the input. If it can produce a valid
// projection, `visit` calls `receive` once with the Abstract representing the
// imported graph and returns Satisfied after the callback finishes.
//
// The caller can supply callback state through receiver, which is passed
// unchanged to `receive`. The input, Representation and receiver state remain
// available until `visit` returns. The imported graph is available during the
// callback, where the consumer can inspect it, copy data or negotiate retained
// access through the graph's capabilities.
//
// A `visit` returning Unknown or Rejected guarantees that `receive` was never
// called. Unknown leaves the import request undetermined. Rejected explicitly
// refuses it. The default implementation supplies None for every input, with
// zero routes to enumerate, and returns Satisfied.
typedef struct ttx_import {
  void* source;
  const struct ttx_import_ops* operations;
} ttx_import;

typedef struct ttx_import_ops {
  ttx_abstract_ops abstract;
  ttx_binding_status (*visit)(
      void* source,
      const void* input,
      const ttx_representation* representation,
      void* receiver,
      void (*receive)(void* receiver, ttx_abstract subject));
} ttx_import_ops;

C_LINKAGE const ttx_representation* ttx_import_representation(void);
#endif
