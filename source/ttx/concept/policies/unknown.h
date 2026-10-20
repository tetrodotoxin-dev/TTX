// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_POLICIES_UNKNOWN_H
#define TTX_CONCEPT_POLICIES_UNKNOWN_H

#include "ttx/concept/abstract.h"

// Unknown means the provider has not determined this route's result for the
// current observation. Binding the policy returns its Abstract interface.
// The shared implementation from `ttx_unknown` returns itself for further
// routes.
//
// To try again, the consumer resolves the same route on the original Abstract.
// The provider evaluates that new request under its current policy.
#define TTX_UNKNOWN_ID_HIGH 0x84c6c053b3254f7aULL
#define TTX_UNKNOWN_ID_LOW 0xa873409b31bcf608ULL

C_LINKAGE EXPORTED(TTX)
ttx_abstract ttx_unknown(void);

#endif
