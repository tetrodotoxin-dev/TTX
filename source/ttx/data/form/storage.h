// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_FORM_STORAGE_H
#define TTX_DATA_FORM_STORAGE_H

#include "ttx/data/form/representation.h"

// Storage pairs a described region with its available capacity. Operations
// establish whether that region supplies input or receives output. The caller
// keeps the record, representation and region accessible through each use.
// Copying this record copies its pointers; it leaves the referenced data and
// the supplying access contract in place.
typedef struct ttx_storage {
  const ttx_representation* representation;
  U8* data;
  Count size;
} ttx_storage;

// Admission checks capacity and alignment against the prepared geometry.
// Borrowing storage for another result needs no further compilation or walk
// through the immutable description.
C_LINKAGE EXPORTED(TTX)
ttx_data_status ttx_storage_check(ttx_storage storage);

#endif
