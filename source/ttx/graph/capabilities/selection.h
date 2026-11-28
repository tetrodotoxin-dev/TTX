// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_CAPABILITIES_SELECTION_H
#define TTX_GRAPH_CAPABILITIES_SELECTION_H

#include "ttx/graph/abstract.h"

#define TTX_SELECTION_ID_HIGH 0x84e9cf0b6c6f44c6ULL
#define TTX_SELECTION_ID_LOW 0xb06ad2db4fd6476fULL

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

#ifdef __cplusplus

namespace Ttx::Graph::Capabilities {

class Selection {
 public:
  static constexpr auto contract_id = Perimortem::Core::Identity::Uuid(
      TTX_SELECTION_ID_HIGH,
      TTX_SELECTION_ID_LOW);
  using Api = ttx_selection;
  using Operations = ttx_selection_ops;

  // Copies the complete C record supplied under the Selection contract.
  // Explicit construction preserves the provider's context and operations as
  // one API, with their supplying access duration.
  explicit constexpr Selection(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  auto supports(Perimortem::Core::Identity::Uuid policy) const
      -> Semantic::Negotiation::Status {
    return Semantic::Negotiation::Status(
        api.operations->supports(api.context, policy));
  }

  template <typename Policy>
  auto supports() const -> Semantic::Negotiation::Status {
    return supports(Policy::contract_id);
  }

  // Applies the request graph as consumer context for one visitation
  // interpreted by the provider. A satisfied result can be empty. Unknown and
  // Rejected invoke no visitor.
  auto select(Abstract request, Abstract::Visitor visitor) const
      -> Semantic::Negotiation::Status {
    const auto selected = get_abi();
    const ttx_graph_visitor callback(visitor_context(visitor), visit);
    return Semantic::Negotiation::Status(
        selected.operations->select(selected.context, request, callback));
  }

 private:
  static auto visitor_context(Abstract::Visitor& visitor) -> void* {
    return &visitor;
  }

  static auto visit(
      void* context,
      perimortem_view_bytes route,
      ttx_abstract value) -> void {
    auto& visitor = *Perimortem::Core::Data::cast<Abstract::Visitor>(context);
    visitor(Perimortem::Core::View::Bytes(route), Abstract(value));
  }

  Api api;
};

}  // namespace Ttx::Graph::Capabilities

#endif

#endif
