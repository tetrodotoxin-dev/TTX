// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_CAPABILITIES_CREATE_H
#define TTX_CONCEPT_CAPABILITIES_CREATE_H
#include "ttx/concept/abstract.h"

#define TTX_CREATE_ID_HIGH 0x5b2cd19ec46d4873ULL
#define TTX_CREATE_ID_LOW 0xa9d0c52dd6f2660aULL

// Exposes the capability to create an Abstract from the supplied argument
// graph. The provider interprets the arguments through their routes and
// contracts. None represents intentionally absent arguments. If creation
// succeeds, `create` calls `callback` once with the created Abstract and
// returns Satisfied after the callback finishes.
//
// The caller can supply callback state through `callback_context`, which is
// passed unchanged to `callback`. The arguments and callback state remain
// available until `create` returns. The created Abstract is available during
// the callback. The consumer can use its capabilities, copy observations or
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
  ttx_abstract_ops abstract;
  ttx_binding_status (*create)(
      void* context,
      ttx_abstract arguments,
      void* callback_context,
      void (*callback)(void* context, ttx_abstract abstract));
} ttx_create_ops;

C_LINKAGE const ttx_representation* ttx_create_representation(void);
#endif
