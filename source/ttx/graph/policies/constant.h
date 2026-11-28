// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_CONSTANT_H
#define TTX_GRAPH_POLICIES_CONSTANT_H

#include "ttx/graph/abstract.h"

// Constant lets a consumer reuse a graph selection. A provider supplying
// Constant must preserve the graph location selected by the supplying Abstract
// and route throughout the established access duration. Repeating that route
// request must select the same position. Returning through `get_abstract` must
// preserve that position even when a binding produces a different API record.
// The selected node's data and outgoing observations can still change under
// their own contracts.
#define TTX_CONSTANT_ID_HIGH 0xa9f347a1f048455bULL
#define TTX_CONSTANT_ID_LOW 0xa5d873dbc5e5761fULL

#endif
