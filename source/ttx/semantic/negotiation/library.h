// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_NEGOTIATION_LIBRARY_H
#define TTX_SEMANTIC_NEGOTIATION_LIBRARY_H

#include "ttx/data/form/storage.h"

// A native module exports this data symbol as a const ttx_storage record.
// Its readable payload describes a Query. The host transfers that payload
// against its own Query representation before calling any supplied operation.
// The export record, descriptor bytes and payload remain accessible while
// the module is loaded. The host keeps the module loaded through every use of
// its functions, including releases required by negotiated contracts.
#define TTX_LIBRARY_ENTRY "ttx_entry"

#endif
