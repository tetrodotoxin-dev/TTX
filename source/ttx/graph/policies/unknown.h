// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_UNKNOWN_H
#define TTX_GRAPH_POLICIES_UNKNOWN_H

#include "ttx/graph/abstract.h"

// Unknown marks an undetermined result in graph traversal. Every further route
// returns the same Unknown result. A chain preserves the outcome independently
// of which step reached it. Binding this policy supplies its Abstract.
// `ttx_unknown` provides a shared implementation.
//
// To try again, the consumer resolves the same route on the original Abstract.
// The provider evaluates that new request under its current policy.
C_LINKAGE EXPORTED(TTX)
ttx_abstract ttx_unknown(void);

#endif
