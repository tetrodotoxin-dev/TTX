// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_RETAINED_H
#define TTX_GRAPH_POLICIES_RETAINED_H

#include "ttx/graph/abstract.h"

#define TTX_RETAINED_ID_HIGH 0xcd1fc9de2b8a4663ULL
#define TTX_RETAINED_ID_LOW 0x890b83ffde67368bULL

// Retained keeps an Abstract accessible until the consumer calls `release`.
// The provider keeps that Abstract's context, operation table and referenced
// metadata available for calls made before `release`. The Abstract's
// observations can still change according to its governing policies.
//
// Each successful `retain` call creates one release obligation, including when
// two results use the same context. Copies of a Retained record and interfaces
// bound through its Abstract share that result's release obligation. The
// consumer calls `release` once for each result after finishing all uses of
// those records and interfaces. Using any of them after that release is
// undefined behavior. Another successful `retain` call creates a separate
// obligation whose release is independent.
//
// An operation that requires the consumer to release an Abstract returns
// Retained or another interface that exposes that release obligation. A plain
// Abstract result requires no release from its consumer.
typedef struct ttx_retained {
  void* context;
  const struct ttx_retained_ops* operations;
} ttx_retained;

typedef struct ttx_retained_ops {
  ttx_abstract_ops abstract;
  void (*release)(void* context);
} ttx_retained_ops;

C_LINKAGE EXPORTED(TTX) const ttx_representation* ttx_retained_representation(
    void);

#endif
