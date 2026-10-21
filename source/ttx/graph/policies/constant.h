// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_CONSTANT_H
#define TTX_GRAPH_POLICIES_CONSTANT_H

#include "ttx/graph/abstract.h"

// Constant lets a consumer reuse a graph selection while its supplying access
// remains valid. It fixes the location from which it was bound, so repeating
// the supplying route request selects that same position. Returning
// through `get_abstract` preserves that position even when a binding produces
// a different API record. The selected node's data and outgoing observations
// can still change under their own contracts.
#define TTX_CONSTANT_ID_HIGH 0x3642dd24fadd44ebULL
#define TTX_CONSTANT_ID_LOW 0x97aa4a2e729c28b7ULL

#endif
