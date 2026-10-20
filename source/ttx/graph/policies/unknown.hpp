// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/unknown.h"

namespace Ttx::Graph::Policies {

// Unknown exposes the Abstract returned when the provider leaves a route's
// result undetermined. The shared sentinel from `get_unknown` returns itself
// for further lookups. To observe the provider again, the consumer resolves
// the same route on the original Abstract.
class Unknown {
 public:
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_UNKNOWN_ID_HIGH,
      TTX_UNKNOWN_ID_LOW);
  using Api = ttx_abstract;

  // Copies an Abstract already supplied under the Unknown contract. Explicit
  // construction makes the caller's interpretation visible. Construction
  // performs a record copy, with the policy guaranteed by its provider.
  explicit constexpr Unknown(Api api) : api(api) {}

  // Checks the required pointers through `Abstract::accept`. The caller must
  // supply a readable table with the agreed representation. The provider
  // guarantees the Unknown policy itself.
  static auto accept(Api api) -> Bool { return Abstract::accept(api); }

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract { return Abstract(api); }

  static EXPORTED(TTX) auto get_unknown() -> Unknown;

 private:
  Api api;
};

}  // namespace Ttx::Graph::Policies
