// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_POLICIES_NONE_H
#define TTX_CONCEPT_POLICIES_NONE_H

#include "ttx/concept/abstract.h"

// None represents an explicitly rejected route for the current observation.
// Binding supplies its governing Abstract. `ttx_none` supplies a shared
// implementation whose further routes return the same None result.
//
// The original Abstract and route can select a different result later.
// Combining None with Constant promises that this selection remains None
// throughout the access established by the supplying contract.
#define TTX_NONE_ID_HIGH 0x84c6c053b3254f7aULL
#define TTX_NONE_ID_LOW 0xa873409b31bcf607ULL

C_LINKAGE EXPORTED(TTX)
ttx_abstract ttx_none(void);

#endif
