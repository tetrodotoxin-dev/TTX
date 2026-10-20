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
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_EXPORT_ID_HIGH,
      TTX_EXPORT_ID_LOW);
  using Api = ttx_export;
  using Operations = ttx_export_ops;

  // Checks `expose`, context and the required Abstract operations for null.
  // Any present table must be readable and match the established API
  // representation. The supplying contract establishes Export's meaning.
  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->expose &&
           Abstract::accept(
               ttx_abstract(api.context, &api.operations->abstract));
  }

  // Copies a record already supplied under the Export contract. The caller
  // establishes its representation and required pointers. Explicit construction
  // marks this unchecked conversion from the C record.
  explicit constexpr Export(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract {
    return Abstract(api.context, api.operations->abstract);
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

TTX_DATA_RECORD(
    ttx_export_ops,
    TTX_DATA_MEMBER(ttx_export_ops, abstract),
    TTX_DATA_MEMBER(ttx_export_ops, expose));
TTX_DATA_RECORD(
    ttx_export,
    TTX_DATA_MEMBER(ttx_export, context),
    TTX_DATA_MEMBER(ttx_export, operations));
