// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_CAPABILITIES_SELECTION_H
#define TTX_GRAPH_CAPABILITIES_SELECTION_H

#include "ttx/graph/abstract.h"

#define TTX_SELECTION_ID_HIGH 0x443556d7f7a04e04ULL
#define TTX_SELECTION_ID_LOW 0x809c8f2a78703816ULL

// Selection provides domain-specific visitation by allowing the consumer to
// supply additional context through a semantic graph. Regular visitation is
// neutral to consumer context, while route resolution receives a route as its
// only selection input. Selection resolution remains owned by the provider,
// while the filtering context lets it perform domain-specific optimizations
// that would be lost if the consumer filtered results after visitation.
//
// Selection can distinguish routes that might be aliased without additional
// context. A provider can use that distinction to collapse multiple routes or
// subgraph operations into one efficient visitation set. Selection strengthens
// its callback set into one observation. Satisfied reports that every selected
// occurrence was delivered, including a recognized empty selection. Unknown
// and Rejected deliver no callbacks, distinguishing undetermined and refused
// selections from that empty result.
//
// The first callback begins delivery of the committed occurrence set. Graph
// observations made by a callback cannot add, remove or reorder the remaining
// occurrences in that `select` call. The provider completes every callback
// before returning. Selection transfers no access to the request or visitor
// beyond that synchronous transaction.
//
// Policies checked through Selection apply to every selection it can perform.
// For Ordered, Satisfied means every selection has a meaningful order. Unknown
// means any selection may or may not be ordered. Rejected means selections are
// explicitly unordered and any observed order is coincidental.
typedef struct ttx_selection {
  void* context;
  const struct ttx_selection_ops* operations;
} ttx_selection;

typedef struct ttx_selection_ops {
  // Reports whether every selection establishes `policy`. This operation
  // transfers no interface and each call observes the provider again.
  ttx_negotiation_status (*supports)(void* context, perimortem_uuid policy);

  // Applies the provider's selection policy to `request` and synchronously
  // delivers the resulting occurrences through `visitor` under the contract
  // above.
  ttx_negotiation_status (
      *select)(void* context, ttx_abstract request, ttx_graph_visitor visitor);
} ttx_selection_ops;

#endif
