// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_POLICIES_ORDERED_H
#define TTX_CONCEPT_POLICIES_ORDERED_H

#include "ttx/concept/abstract.h"

// Ordered promises a stable, meaningful visitation order for this Abstract.
// Repeated visits to an unchanged observation preserve the order. A change to
// that order is a meaningful change in the supplying model. Repeated subjects
// remain separate occurrences, so consumers preserve their sequence.
//
// An Unknown answer leaves ordering undetermined by this policy. Rejected
// states that visitation order carries no meaningful information, including
// through other policies supplied by this Abstract. Consumers may then treat
// the sequence as incidental even when repeated visits happen to agree.
//
// `supports` asks for this promise. Binding supplies the governing Abstract
// view whose visitation carries it. Execution and reuse of the visited answers
// follow their own contracts.
#define TTX_ORDERED_ID_HIGH 0x5a3855f4ad5243e7ULL
#define TTX_ORDERED_ID_LOW 0xb049c60e356b0067ULL

#endif
