// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_NEGOTIATION_CALLBACK_H
#define TTX_SEMANTIC_NEGOTIATION_CALLBACK_H

#include "ttx/semantic/negotiation/query.h"

// A provider calls the callback with a Query. The consumer binds
// and uses interfaces inside `callback`, and can ask the provider to retain
// data it needs after the callback returns. The callback returns Satisfied,
// Unknown or Rejected to report the consumer's status for the supplied Query.
typedef struct ttx_query_callback {
  void* context;
  ttx_binding_status (*callback)(void* context, ttx_semantic_query query);
} ttx_query_callback;

#endif
