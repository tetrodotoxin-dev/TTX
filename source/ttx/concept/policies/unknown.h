// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_POLICIES_UNKNOWN_H
#define TTX_CONCEPT_POLICIES_UNKNOWN_H

#include "ttx/concept/abstract.h"

// Unknown represents a route whose result is undetermined for the current
// observation. Binding supplies its governing Abstract. `ttx_unknown` supplies
// a shared implementation whose further routes return the same Unknown result.
//
// A consumer retries resolution through the original Abstract and route.
// The provider evaluates that new observation under its current policy.
#define TTX_UNKNOWN_ID_HIGH 0x84c6c053b3254f7aULL
#define TTX_UNKNOWN_ID_LOW 0xa873409b31bcf608ULL

C_LINKAGE EXPORTED(TTX)
ttx_abstract ttx_unknown(void);

#endif
