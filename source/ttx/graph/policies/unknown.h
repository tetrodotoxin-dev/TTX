// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_UNKNOWN_H
#define TTX_GRAPH_POLICIES_UNKNOWN_H

#include "ttx/graph/abstract.h"

// Unknown represents an unresolved selection in the Abstract graph. It leaves
// open whether the target route can be reached on during future observations.
// The provider makes no promise to eventually produce a target.
//
// Unknown is one of the two axiomatic Abstracts. An Abstract may report
// support for the Unknown contract only if it meets these exact rules:
//
// * It must also support the Abstract contract.
// * It must report unknown for all other contracts.
// * It must only satisfy bind for Abstract.
// * It must report unknown for all other support and binds.
// * It must return indeterminate for it's reachability.
// * Every route from it must return itself.
// * Any observations on the Abstract produces no observable side effects.
//
// Unknown's contract is the formal TTX domain identifier that provides a fast
// path for identification without needing to observe all behaviors. Any
// Abstract can emit support for the Unknown contract must collapse to a perfect
// simulacra so TTX supports a built in Abstract that can be used to statically
// emit for ture indeterminate paths. Consumers however should not assume that the
// static TTX address represents the one and only `Unknown` representation.
C_LINKAGE EXPORTED(TTX)
ttx_abstract ttx_unknown(void);

#endif
