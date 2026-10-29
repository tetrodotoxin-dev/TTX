// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/capabilities/selection.h"

namespace Ttx::Graph::Capabilities {

// Selection adds domain-specific consumer context to ordinary visitation. The
// provider interprets that semantic graph and delivers the complete selection
// as one observation before `select` returns. The portable contract is defined
// in selection.h.
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
    return Semantic::Negotiation::Status(selected.operations->select(
        selected.context, request.get_abi(), callback));
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
