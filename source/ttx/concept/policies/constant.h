// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_POLICIES_CONSTANT_H
#define TTX_CONCEPT_POLICIES_CONSTANT_H

#include "ttx/concept/abstract.h"

// Constant promises that the Abstract and route which supplied this result
// continue to select the same semantic value. The selected value's outgoing
// routes and byte observations establish their own promises independently.
//
// The governing Abstract preserves this policy through further negotiation.
// Storage and executable code remain available through the supplying access
// contract. An acquired Borrowed result carries its separate release
// obligation.
#define TTX_CONSTANT_ID_HIGH 0x84c6c053b3254f7aULL
#define TTX_CONSTANT_ID_LOW 0xa873409b31bcf606ULL

#endif
