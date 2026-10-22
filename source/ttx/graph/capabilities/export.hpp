// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/capabilities/export.h"

namespace Ttx::Graph::Capabilities {

// Uses an Abstract graph to produce a result in another system. The provider
// selects the contracts and routes needed by its target. A graph can supply
// services alongside the content being exported. For example, a terminal can
// use a logger or report generated paths through a capability found on the
// root or one of its routes. Each participating contract defines the
// required services, their arguments and their effect on the exported result.
//
// The graph is available through `expose`. Satisfied reports that the provider
// fulfilled its request, Unknown leaves the outcome undetermined, and Rejected
// refuses it. The provider defines its artifacts and effects, including any
// observations it reports through the supplied graph.
class Export {
 public:
  static constexpr auto contract_id =
      Perimortem::Core::Identity::Uuid(TTX_EXPORT_ID_HIGH, TTX_EXPORT_ID_LOW);
  using Api = ttx_export;
  using Operations = ttx_export_ops;

  // Copies the complete C record supplied under the Export contract.
  // Explicit construction preserves the provider's context and operations
  // as one API, with their supplying access duration.
  explicit constexpr Export(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  // Returns to the graph location that supplied this binding. The provider
  // supplies the complete Abstract and may use a different context and table.
  auto get_abstract() const -> Abstract {
    return Abstract(api.operations->get_abstract(api.context));
  }

  auto expose(Abstract abstract) const -> Semantic::Negotiation::Status {
    const auto api = get_abi();
    return Semantic::Negotiation::Status(
        api.operations->expose(api.context, abstract.get_abi()));
  }

 private:
  Api api;
};

}  // namespace Ttx::Graph::Capabilities
