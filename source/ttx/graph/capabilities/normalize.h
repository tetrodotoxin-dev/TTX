// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_CAPABILITIES_NORMALIZE_H
#define TTX_GRAPH_CAPABILITIES_NORMALIZE_H

#include "ttx/graph/abstract.h"

#define TTX_NORMALIZE_ID_HIGH 0x1f4ef852d72e4e5aULL
#define TTX_NORMALIZE_ID_LOW 0x98386a0d829aaf4cULL

// Simulacra such as Alias can have different semantic and nominal forms. The
// semantic graph location of an abstract is globally unique (`get_abstract`
// always represents the actual Abstract), but nominal locations of intent can
// be an arbitary location and may be aliased.
//
// If an Abstract provides the Normalize capability then it provides a resolve
// operation such that:
//
//  `abstract.get_abstract() != abstract.resolve().get_abstract()`
//
// The actual resolution policy and definition of resolve is provider defined.
typedef struct ttx_normalize {
  void* context;
  const struct ttx_normalize_ops* operations;
} ttx_normalize;

typedef struct ttx_normalize_ops {
  // On Satisfied the provider writes one accessible Abstract to output that is
  // different than the Abstract that represents the provider itself in a single
  // observation. Resolution policy is provider defined by the provider and only
  // promises the Abstract contract for its output.
  //
  // The returned value can be used or cached but the provider has no obligation
  // to use or return the same Abstract for future observations.
  ttx_negotiation_status (*resolve)(void* context, ttx_abstract* output);
} ttx_normalize_ops;

#endif
