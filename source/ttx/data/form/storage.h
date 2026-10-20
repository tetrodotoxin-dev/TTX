// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_FORM_STORAGE_H
#define TTX_DATA_FORM_STORAGE_H

#include "ttx/data/form/representation.h"

// Storage describes a buffer used to send or receive a value. Its
// Representation gives the value's layout and required alignment. `size` is
// the number of available bytes at `data`, which can exceed the value's extent.
// Transfer can then check that two buffers describe compatible values and have
// enough space before copying. The operation receiving Storage determines
// whether it reads or writes the buffer.
//
// Copying Storage copies the record, leaving the buffer and description in
// place. The caller keeps the buffer, Representation and its descriptor bytes
// accessible for every use. Copying the record does not extend their lifetime.
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
