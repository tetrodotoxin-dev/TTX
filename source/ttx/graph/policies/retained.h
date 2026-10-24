// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_RETAINED_H
#define TTX_GRAPH_POLICIES_RETAINED_H

#include "ttx/graph/abstract.h"

#define TTX_RETAINED_ID_HIGH 0xff314d3912944f68ULL
#define TTX_RETAINED_ID_LOW 0x86752b0f31b71874ULL

// Release reports the cleanup required by one acquisition. Success means that
// cleanup completed, including when other acquisitions keep the resource
// alive. CleanupFailed means that the provider could not complete that cleanup.
// Both outcomes consume the acquisition. The status remains usable after the
// provider releases its state because the result contains only a value.
typedef U8 ttx_release_status;

#define TTX_RELEASE_SUCCESS ((ttx_release_status)0)
#define TTX_RELEASE_CLEANUP_FAILED ((ttx_release_status)1)

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
// The provider reports Success or CleanupFailed from `release`. Each outcome
// ends this acquisition's access, including access through its copied records
// and bound interfaces. Other acquisitions keep their own access durations.
// The provider includes cleanup required by its dependent resources in the
// reported outcome. A consumer handles failure through that returned status
// after completing its use of the released interfaces.
//
// An operation that requires the consumer to release an Abstract returns
// Retained or another interface that exposes that release obligation. A plain
// Abstract result requires no release from its consumer.
typedef struct ttx_retained {
  void* context;
  const struct ttx_retained_ops* operations;
} ttx_retained;

typedef struct ttx_retained_ops {
  // Supplies the retained Abstract with its own context and operations. A
  // record obtained by binding returns to that binding's graph location.
  ttx_abstract (*get_abstract)(void* context);
  ttx_release_status (*release)(void* context);
} ttx_retained_ops;

#endif
