// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_CAPABILITIES_RETAIN_H
#define TTX_GRAPH_CAPABILITIES_RETAIN_H

#include "ttx/graph/policies/retained.h"

#define TTX_RETAIN_ID_HIGH 0xd576263608ac45a2ULL
#define TTX_RETAIN_ID_LOW 0x9e605b7d64c8a413ULL

// Retain asks the provider to supply an Abstract that remains usable after
// the current observation ends. The returned Retained interface carries the
// consumer's release obligation. The provider can keep the original state
// available or supply different state that preserves the Abstract's policies.
//
// On success, `retain` writes a complete ttx_retained record to `output` and
// returns Satisfied. The consumer calls that record's `release` function once
// after its last use of the returned Abstract. Each successful call creates a
// separate release obligation, even when several results use the same context.
//
// Unknown means the provider leaves its ability to supply that Retained
// interface undetermined. Rejected means the provider refuses to supply it.
// The caller uses `output` only after `retain` returns Satisfied.
typedef struct ttx_retain {
  void* context;
  const struct ttx_retain_ops* operations;
} ttx_retain;

typedef struct ttx_retain_ops {
  ttx_abstract_ops abstract;
  ttx_negotiation_status (*retain)(void* context, ttx_retained* output);
} ttx_retain_ops;

C_LINKAGE EXPORTED(TTX) const ttx_representation* ttx_retain_representation(
    void);

#endif
