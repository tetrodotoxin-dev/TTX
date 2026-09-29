// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_POLICIES_UNKNOWN_H
#define TTX_CONCEPT_POLICIES_UNKNOWN_H

#include "ttx/concept/abstract.h"

// Unknown answers a question whose result is undetermined for this observation.
// Binding returns its Abstract view. ttx_unknown supplies the shared
// Abstract, whose further routes continue to answer Unknown.
//
// A consumer seeking another observation asks the original provider again.
#define TTX_UNKNOWN_ID_HIGH 0x84c6c053b3254f7aULL
#define TTX_UNKNOWN_ID_LOW 0xa873409b31bcf608ULL

PERIMORTEM_C ttx_abstract ttx_unknown(void);

#endif
