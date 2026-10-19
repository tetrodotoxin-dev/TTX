// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_FORM_STORAGE_H
#define TTX_DATA_FORM_STORAGE_H

#include "ttx/data/form/representation.h"

// Storage supplies a described data region for copying values or receiving a
// negotiated API. Representation supplies the value's extent and alignment,
// while size is the capacity of the region at data.
// The receiving operation determines whether that region is read or written.
//
// The caller keeps the representation, descriptor bytes and data region
// accessible through use. Copying Storage copies its pointers and capacity.
// Referenced data retains the access duration established by its supplier.
typedef struct ttx_storage {
  const ttx_representation* representation;
  U8* data;
  Count size;
} ttx_storage;

// Checks capacity and data alignment using an already canonical Representation.
// A missing representation returns Invalid. A present representation must have
// readable descriptor bytes satisfying its decoding contract. Insufficient
// capacity returns Bounds. Nonempty values require a nonnull data address, and
// a misaligned address returns Invalid. Zero extent permits null data.
//
// This checks declared geometry. The caller establishes actual memory access
// and keeps the described region accessible through the operation.
C_LINKAGE EXPORTED(TTX)
ttx_data_status ttx_storage_check(ttx_storage storage);

#endif
