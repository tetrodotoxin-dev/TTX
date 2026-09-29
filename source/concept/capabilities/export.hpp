// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once
#include "ttx/concept/abstract.hpp"
#include "ttx/concept/capabilities/export.h"

namespace Ttx::Concept::Capabilities {

// Uses an Abstract graph to produce a result in another system. The exporter
// selects the contracts and routes needed by its target. A graph can supply
// services alongside the content being exported. For example, a terminal can
// use a logger or report generated paths through a capability found on the
// root or one of its routes. Each participating contract defines those
// questions and whether their answers are required to produce the result.
//
// The graph is available through expose. Satisfied reports that the exporter
// fulfilled its request, Unknown leaves the outcome undetermined, and Rejected
// refuses it. The exporter defines its artifacts and effects, including any
// observations it reports through the supplied graph.
class Export : public Abstract {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_EXPORT_ID_HIGH, TTX_EXPORT_ID_LOW);
  using Api = ttx_export;
  using Operations = ttx_export_ops;
  static auto accept(Api value) -> Bool {
    return value.operations && value.operations->expose &&
           Abstract::accept(
               ttx_abstract(value.source, &value.operations->abstract));
  }
  explicit constexpr Export(Api value)
      : Abstract(value.source, value.operations->abstract) {}
  constexpr auto get_abi() const -> Api {
    const auto value = Abstract::get_abi();
    return Api(
        value.source, reinterpret_cast<const Operations*>(value.operations));
  }
  auto expose(Abstract subject) const
      -> Semantic::Negotiation::Binding::Status {
    const auto api = get_abi();
    return static_cast<Semantic::Negotiation::Binding::Status>(
        api.operations->expose(api.source, subject.get_abi()));
  }

  template <typename Owner>
    requires(!__is_base_of(Abstract, Owner))
  static auto provide(const Owner& owner) -> Export {
    static const Operations operations = Operations(
        *Abstract::provide(owner).get_abi().operations,
        [](const void* source, ttx_abstract subject) -> ttx_binding_status {
          return static_cast<ttx_binding_status>(
              static_cast<const Owner*>(source)->expose(Abstract(subject)));
        });
    return Export(Api(&owner, &operations));
  }
};

}  // namespace Ttx::Concept::Capabilities

TTX_DATA_RECORD(
    ttx_export_ops,
    TTX_DATA_MEMBER(ttx_export_ops, abstract),
    TTX_DATA_MEMBER(ttx_export_ops, expose));
TTX_DATA_RECORD(
    ttx_export,
    TTX_DATA_MEMBER(ttx_export, source),
    TTX_DATA_MEMBER(ttx_export, operations));
