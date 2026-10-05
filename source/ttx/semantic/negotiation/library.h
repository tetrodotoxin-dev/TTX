// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_NEGOTIATION_LIBRARY_H
#define TTX_SEMANTIC_NEGOTIATION_LIBRARY_H

#include "ttx/semantic/negotiation/callback.h"

// Entry point for a dynamically loaded TTX provider. The loader calls
// `ttx_query` with the host's services and a callback. The provider passes its
// Query to that callback and keeps the Query valid until the callback returns.
//
// The caller keeps the DLL or SO loaded while using its interfaces, including
// calls that release retained data. The entry returns the callback's status,
// or Unknown or Rejected if it declines to supply a Query.
#define TTX_LIBRARY_ENTRY "ttx_query"
typedef ttx_binding_status (
    *ttx_library_entry)(ttx_semantic_query host, ttx_query_callback callback);

#endif
