// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_FORM_STORAGE_H
#define TTX_DATA_FORM_STORAGE_H

#include "ttx/data/form/representation.h"

// Representation supplies the geometry, while Storage supplies a particular
// destination for an observation. Pairing that prepared description with the
// caller's region lets two results satisfy the same Representation while their
// buffers have independent lifetimes.
//
// This record borrows both the representation and the destination bytes. The
// caller keeps them alive through every operation using the view. Copies
// share the same storage and lifetime. A deferred
// execution policy must arrange that longer lifetime before handing the view
// to a later call.
typedef struct ttx_storage {
  const ttx_representation* representation;
  U8* data;
  Count size;
} ttx_storage;

// Admission checks capacity and alignment against the prepared geometry.
// Borrowing storage for another result needs no further compilation or walk
// through the immutable description.
PERIMORTEM_C ttx_data_status ttx_storage_check(ttx_storage storage);

#endif
