// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_NONE_H
#define TTX_GRAPH_POLICIES_NONE_H

#include "ttx/graph/abstract.h"

// None marks an explicit dead end in graph traversal. Every further route
// returns the same None result. A chain preserves the outcome independently
// of which step reached it. Binding this policy supplies its Abstract.
// None satisfies Constant because observations through that returned location
// cannot leave the dead end. It rejects every unrelated contract UUID.
// `ttx_none` provides a shared implementation of this complete result.
//
// Constant belongs to the None location. The original Abstract and route can
// select a different result later unless that selection independently supplies
// its own stability promise.
C_LINKAGE EXPORTED(TTX)
ttx_abstract ttx_none(void);

#endif
