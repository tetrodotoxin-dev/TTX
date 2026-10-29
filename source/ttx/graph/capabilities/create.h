// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_CAPABILITIES_CREATE_H
#define TTX_GRAPH_CAPABILITIES_CREATE_H

#include "ttx/graph/abstract.h"

#define TTX_CREATE_ID_HIGH 0x34b34c662f3b492cULL
#define TTX_CREATE_ID_LOW 0xb30abce0b7a79738ULL

// Exposes the capability to create an Abstract from the supplied argument
// graph. The provider interprets the arguments through their routes and
// contracts. None represents intentionally absent arguments. If creation
// succeeds, `create` calls `callback` once with the created Abstract and
// returns Satisfied after the callback finishes.
//
// The caller can supply callback state through `callback_context`, which is
// passed unchanged to `callback`. `create` observes the argument graph and
// invokes the callback within one synchronous transaction. It transfers no
// access to either input beyond return. The created Abstract is available
// during the callback. The consumer can use its capabilities or copy typed
// results before returning, allowing providers to create objects in temporary
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
  ttx_negotiation_status (*create)(
      void* context,
      ttx_abstract arguments,
      void* callback_context,
      void (*callback)(void* callback_context, ttx_abstract abstract));
} ttx_create_ops;

#endif
