// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_NONE_H
#define TTX_GRAPH_POLICIES_NONE_H

#include "ttx/graph/abstract.h"

// None reports that a selection has no target in this observation. A later
// lookup at the original location may return a different result. A caller
// can reuse a route selection when its provider independently grants Constant.
//
// An Abstract supplying None supports None and Abstract and reports
// Unreachable. Binding Abstract returns its own location. Binding None
// returns Rejected. Resolution and every route lead back to this location,
// visitation invokes zero callbacks, and these observations have no visible
// side effects. Additional policies carry their own obligations.
//
// Check support for None to recognize the policy on independently supplied
// records. The function `ttx_none()` returns the library's shared
// implementation, which declines Constant.
C_LINKAGE EXPORTED(TTX)
ttx_abstract ttx_none(void);

#endif
