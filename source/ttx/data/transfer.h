// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_TRANSFER_H
#define TTX_DATA_TRANSFER_H

#include "ttx/data/form/storage.h"

// Transfer copies a represented value between two regions. The destination's
// canonical form is already admitted. The source descriptor bytes are readable
// for their declared size; agreement with the destination establishes their
// form before Transfer interprets their geometry.
//
// Both regions must have the capacity and alignment required by that form.
// Success copies exactly its extent, including when the regions overlap.
// Spare capacity remains untouched. A failed check leaves the destination
// unchanged. Empty representations transfer successfully without data pointers.
//
// The caller supplies accessible source and destination memory for this call.
// Copying a record preserves the contracts of any pointers it contains. Their
// supplying contracts establish access independently of this byte transfer.
C_LINKAGE EXPORTED(TTX)
ttx_data_status ttx_transfer(
    const ttx_storage* input,
    const ttx_storage* output);

#endif
