// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_POLICIES_CONSTANT_H
#define TTX_CONCEPT_POLICIES_CONSTANT_H

#include "ttx/concept/abstract.h"

// Constant promises the same answer for this provider and semantic question
// throughout the graph lifetime. Consumers can reuse that answer while its
// supplying storage remains available. Each outgoing question has its own
// promise.
//
// Binding returns the governing Abstract view, so further questions continue
// through that policy. Storage retention follows its own contract. An acquired
// Borrowed answer still requires release when its caller finishes using it.
#define TTX_CONSTANT_ID_HIGH 0x84c6c053b3254f7aULL
#define TTX_CONSTANT_ID_LOW 0xa873409b31bcf606ULL

#endif
