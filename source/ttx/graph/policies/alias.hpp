// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/alias.h"

namespace Ttx::Graph::Policies {

// Bind Alias on a reference location to inspect or redirect its referent.
// Copies of Alias share the provider's reference, so a successful write
// through one copy is visible on the next read through another. To visit
// the current target, read an Abstract with get_referent() and call
// visit_routes() on that Abstract. A callback can change the reference
// while the captured Abstract continues the visit at the original provider.
// A caller needing the final subject can bind Capabilities::Normalize on
// the initialized slot. Its resolve operation leaves the immediate links
// available to another caller inspecting them with Alias. The C contract
// in alias.h specifies the access granted to each record.
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
    return Abstract(api.operations->get_referent(api.context));
  }

  // The provider returns Satisfied when it publishes target. A read-only
  // provider may refuse a write with Rejected. A provider that cannot
  // decide returns Unknown. Neither unsuccessful request redirects the Alias.
  auto set_referent(Abstract target) const -> Semantic::Negotiation::Status {
    return Semantic::Negotiation::Status(
        api.operations->set_referent(api.context, target.get_abi()));
  }

  // The owner supplies writable storage for a slot initialized to Unknown.
  // The returned Abstract uses the address of slot as its context, so the
  // owner must keep slot at that address while callers have access to it.
  static auto initialize(ttx_alias_slot& slot) -> Abstract {
    return Abstract(ttx_alias_initialize(&slot));
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
