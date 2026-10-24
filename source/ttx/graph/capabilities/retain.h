// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_CAPABILITIES_RETAIN_H
#define TTX_GRAPH_CAPABILITIES_RETAIN_H

#include "ttx/graph/policies/retained.h"

#define TTX_RETAIN_ID_HIGH 0x61c32d95c4c24501ULL
#define TTX_RETAIN_ID_LOW 0x8d7495cbbf143723ULL

// Retain asks the provider to supply an Abstract that remains usable after
// its current access duration ends. The returned Retained interface carries
// the consumer's release obligation. The provider can keep the original state
// available or supply different state that preserves the Abstract's policies.
//
// On success, `retain` writes a complete ttx_retained record to `output` and
// returns Satisfied. The consumer calls that record's `release` function once
// after its last use of the returned Abstract. Each successful call creates a
// separate release obligation, even when several results use the same context.
// That release consumes the acquisition and reports its cleanup outcome under
// the Retained contract.
//
// Unknown means the provider leaves its ability to supply that Retained
// interface undetermined. Rejected means the provider refuses to supply it.
// The caller uses `output` only after `retain` returns Satisfied.
typedef struct ttx_retain {
  void* context;
  const struct ttx_retain_ops* operations;
} ttx_retain;

typedef struct ttx_retain_ops {
  // Supplies the graph location from which this contract was bound, with its
  // own context and operations. Repeated bind/get_abstract round trips preserve
  // that location even when the provider publishes different API records.
  ttx_abstract (*get_abstract)(void* context);
  ttx_negotiation_status (*retain)(void* context, ttx_retained* output);
} ttx_retain_ops;

#endif
