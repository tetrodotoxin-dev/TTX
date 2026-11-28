// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_ALIAS_H
#define TTX_GRAPH_POLICIES_ALIAS_H

#include "ttx/graph/abstract.h"

#define TTX_ALIAS_ID_HIGH 0x3049a145302d432fULL
#define TTX_ALIAS_ID_LOW 0xa4bf1f457f1000efULL

// An Alias represents its own reachable location. It can supply local binds
// and routes, including names and documentation belonging to that position.
// Binding Abstract returns the Alias. Binding Alias lets a caller inspect
// one link or request a new target. Abstract's resolve operation follows
// the link to the subject for this observation.
//
// The setter accepts any complete Abstract as a target, including None,
// Unknown and the Alias itself, subject to the provider's write policy.
// A resolution cycle yields Unknown and preserves the links for inspection.
typedef struct ttx_alias {
  void* context;
  const struct ttx_alias_ops* operations;
} ttx_alias;

typedef struct ttx_alias_ops {
  // Every permitted read returns the immediate stored referent as a complete,
  // accessible Abstract by value. If the Alias points to itself, the caller
  // reads back the Alias. A caller that captured a referent keeps its original
  // access after a redirect.
  ttx_abstract (*get_referent)(void* context);

  // The setter rejects an incomplete target. The provider returns Satisfied
  // when it publishes the new referent. An unsuccessful write preserves the
  // previous referent.
  ttx_negotiation_status (*set_referent)(void* context, ttx_abstract target);
} ttx_alias_ops;

#endif
