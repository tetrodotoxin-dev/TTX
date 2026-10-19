// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_POLICIES_ORDERED_H
#define TTX_CONCEPT_POLICIES_ORDERED_H

#include "ttx/concept/abstract.h"

// Ordered gives meaning to the sequence produced by Abstract visitation.
// Repeated visits to unchanged provider state preserve that sequence. Changes
// in order represent changes in the provider's model. Repeated Abstracts
// remain separate occurrences, so consumers preserve their positions.
//
// Unknown leaves the ordering promise undetermined. Rejected establishes that
// order carries no meaning, including through the other policies supplied by
// this Abstract. An identical sequence across visits can still be incidental.
//
// Binding supplies the governing Abstract whose visitation has these rules.
// The visited Abstracts keep the access guarantees of their own contracts.
#define TTX_ORDERED_ID_HIGH 0x5a3855f4ad5243e7ULL
#define TTX_ORDERED_ID_LOW 0xb049c60e356b0067ULL

#endif
