// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_POLICIES_NONE_H
#define TTX_CONCEPT_POLICIES_NONE_H

#include "ttx/concept/abstract.h"

// None answers a route with explicit rejection. Binding returns its Abstract
// view. `ttx_none` supplies the shared Abstract, whose further routes return
// that same answer.
//
// Rejection describes the observation that produced it. A provider can combine
// None with Constant to promise rejection for that edge's entire graph
// lifetime.
#define TTX_NONE_ID_HIGH 0x84c6c053b3254f7aULL
#define TTX_NONE_ID_LOW 0xa873409b31bcf607ULL

PERIMORTEM_C ttx_abstract ttx_none(void);

#endif
