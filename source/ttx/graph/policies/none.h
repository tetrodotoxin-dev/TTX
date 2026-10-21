// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_NONE_H
#define TTX_GRAPH_POLICIES_NONE_H

#include "ttx/graph/abstract.h"

// None marks an explicit dead end in graph traversal. Every further route
// returns the same None result. A chain preserves the outcome independently
// of which step reached it. Binding this policy supplies its Abstract.
// `ttx_none` provides a shared implementation.
//
// The original Abstract and route can select a different result later.
// Combining None with Constant promises that this selection remains None
// throughout the access established by the supplying contract.
C_LINKAGE EXPORTED(TTX)
ttx_abstract ttx_none(void);

#endif
