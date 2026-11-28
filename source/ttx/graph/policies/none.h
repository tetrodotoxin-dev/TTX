// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_NONE_H
#define TTX_GRAPH_POLICIES_NONE_H

#include "ttx/graph/abstract.h"

// None represents a known durable dead end in the Abstract graph. It does not
// mark that an observation is empty, but instead signals that there is not and
// NEVER will be a location at the observed location in the graph: the requested
// location is permanently unreachable.
//
// The None contract represents one of the two axiomatic Abstracts and can only
// be reported as supported if the Abstract meets these exact policies:
//
// * It must also support both Constant and Abstract contracts.
// * It must explicitly reject all other contracts.
// * It must only satisfy bind for Abstract.
// * It must explicitly reject all other binds.
// * It must return Unreachable for it's reachability.
// * Every route from it must return itself.
// * Any observations on the Abstract produces no observable side effects.
//
// None's contract is the formal TTX domain identifier that provides a fast path
// for identification without needing to observe all behaviors. Any Abstract can
// emit support for the None contract must collapse to a perfect simulacra so
// TTX supports a built in Abstract that can be used to statically emit for ture
// unreachable paths. Consumers however should not assume that the static TTX
// address represents the one and only `None` representation.
C_LINKAGE EXPORTED(TTX)
ttx_abstract ttx_none(void);

#endif
