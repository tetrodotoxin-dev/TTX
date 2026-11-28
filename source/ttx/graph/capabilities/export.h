// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_CAPABILITIES_EXPORT_H
#define TTX_GRAPH_CAPABILITIES_EXPORT_H

#include "ttx/graph/abstract.h"

#define TTX_EXPORT_ID_HIGH 0xd22a0290a2c745a6ULL
#define TTX_EXPORT_ID_LOW 0x95ea300e0ae4378eULL

// Uses an Abstract graph to produce a result in another system. The provider
// selects the contracts and routes needed by its target. A graph can supply
// services alongside the content being exported. For example, a terminal can
// use a logger or report generated paths through a capability found on the
// root or one of its routes. Each participating contract defines the
// required services, their arguments and their effect on the exported result.
//
// `expose` performs its graph observations and effects within one synchronous
// transaction and transfers no access to the supplied Abstract beyond return.
// Satisfied reports that the provider fulfilled its request, Unknown leaves
// the outcome undetermined, and Rejected refuses it. The provider defines its
// artifacts and effects, including any observations it reports through the
// supplied graph.
typedef struct ttx_export {
  void* context;
  const struct ttx_export_ops* operations;
} ttx_export;

typedef struct ttx_export_ops {
  ttx_negotiation_status (*expose)(void* context, ttx_abstract abstract);
} ttx_export_ops;

#ifdef __cplusplus

namespace Ttx::Graph::Capabilities {

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

  auto expose(Abstract abstract) const -> Semantic::Negotiation::Status {
    const auto api = get_abi();
    return Semantic::Negotiation::Status(
        api.operations->expose(api.context, abstract));
  }

 private:
  Api api;
};

}  // namespace Ttx::Graph::Capabilities

#endif

#endif
