// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/alias.h"

namespace Ttx::Graph::Policies {

// Bind Alias on a reference location to inspect or redirect its referent.
// Copies of Alias share the provider's reference. Each get_referent() reads
// the immediate link. To visit the current target, call visit_routes() on
// that captured Abstract. A callback can redirect the Alias while the visit
// continues at the original provider.
// A caller needing the subject calls resolve() on the graph location.
class Alias {
 public:
  static constexpr auto contract_id =
      Perimortem::Core::Identity::Uuid(TTX_ALIAS_ID_HIGH, TTX_ALIAS_ID_LOW);
  using Api = ttx_alias;
  using Operations = ttx_alias_ops;

  // Construct from a complete C API record supplied under the Alias contract.
  // The wrapper borrows the provider's context and operations for the
  // access duration granted when the provider supplied the record.
  explicit constexpr Alias(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  auto get_referent() const -> Abstract {
    return api.operations->get_referent(api.context);
  }

  // An incomplete target yields Rejected. The
  // provider returns Satisfied when it publishes target. A provider that
  // refuses the write returns Rejected. One that cannot decide returns
  // Unknown. Both unsuccessful outcomes preserve the stored referent.
  auto set_referent(Abstract target) const -> Semantic::Negotiation::Status {
    return Semantic::Negotiation::Status(
        api.operations->set_referent(api.context, target));
  }

 private:
  Api api;
};

}  // namespace Ttx::Graph::Policies

TTX_DATA_RECORD(
    ttx_alias_ops,
    TTX_DATA_MEMBER(ttx_alias_ops, get_referent),
    TTX_DATA_MEMBER(ttx_alias_ops, set_referent));
TTX_DATA_RECORD(
    ttx_alias,
    TTX_DATA_MEMBER(ttx_alias, context),
    TTX_DATA_MEMBER(ttx_alias, operations));
