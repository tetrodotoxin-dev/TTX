// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_CAPABILITIES_CREATE_H
#define TTX_GRAPH_CAPABILITIES_CREATE_H

#include "ttx/graph/abstract.h"

#define TTX_CREATE_ID_HIGH 0x38a9c8cd0a4f41caULL
#define TTX_CREATE_ID_LOW 0xb8b6e802ba34fd56ULL

// Exposes the capability to create an Abstract from the supplied argument
// graph. The provider interprets the arguments through their routes and
// contracts. None represents intentionally absent arguments. If creation
// succeeds, `create` calls `callback` once with the created Abstract and
// returns Satisfied after the callback finishes.
//
// The caller can supply callback state through `callback_context`, which is
// passed unchanged to `callback`. The arguments and callback state remain
// available until `create` returns. The created Abstract is available during
// the callback. The consumer can use its capabilities, copy typed results or
// negotiate retained access, allowing providers to create objects in temporary
// storage.
//
// A `create` call returning Unknown or Rejected guarantees that `callback` was
// never called. Unknown leaves the creation request undetermined. Rejected
// explicitly refuses it.
typedef struct ttx_create {
  void* context;
  const struct ttx_create_ops* operations;
} ttx_create;

typedef struct ttx_create_ops {
  // Supplies the graph location from which this contract was bound, with its
  // own context and operations. Repeated bind/get_abstract round trips preserve
  // that location even when the provider publishes different API records.
  ttx_abstract (*get_abstract)(void* context);
  ttx_negotiation_status (*create)(
      void* context,
      ttx_abstract arguments,
      void* callback_context,
      void (*callback)(void* callback_context, ttx_abstract abstract));
} ttx_create_ops;

#endif
