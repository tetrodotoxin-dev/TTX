// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_NEGOTIATION_RECEIVER_H
#define TTX_SEMANTIC_NEGOTIATION_RECEIVER_H

#include "ttx/semantic/negotiation/query.h"

// Receives a Query during a synchronous provider call. The consumer binds
// and uses interfaces inside `receive`, and can ask the provider to retain data
// it needs after the callback returns. The callback returns Satisfied, Unknown
// or Rejected to report the consumer's answer to the supplied Query.
typedef struct ttx_query_receiver {
  void* source;
  ttx_binding_status (*receive)(void* source, ttx_semantic_query subject);
} ttx_query_receiver;

#endif
