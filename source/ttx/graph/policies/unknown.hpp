// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/unknown.h"

namespace Ttx::Graph::Policies {

// Unknown represents an unresolved selection in the Abstract graph. It leaves
// open whether the target route can be reached on during future observations.
// The provider makes no promise to eventually produce a target.
//
// Unknown is one of the two axiomatic Abstracts. An Abstract may report
// support for the Unknown contract only if it meets these exact rules:
//
// * It must also support the Abstract contract.
// * It must report unknown for all other contracts.
// * It must only satisfy bind for Abstract.
// * It must report unknown for all other support and binds.
// * It must return indeterminate for it's reachability.
// * Every route from it must return itself.
// * Any observations on the Abstract produces no observable side effects.
//
// Unknown's contract is the formal TTX domain identifier that provides a fast
// path for identification without needing to observe all behaviors. Any
// Abstract can emit support for the Unknown contract must collapse to a perfect
// simulacra so TTX supports a built in Abstract that can be used to statically
// emit for indeterminate paths. Consumers however should not assume that the
// static TTX address represents the one and only `Unknown` representation.
class Unknown {
 public:
  static constexpr auto contract_id =
      Perimortem::Core::Identity::Uuid(TTX_UNKNOWN_ID_HIGH, TTX_UNKNOWN_ID_LOW);
  using Api = ttx_abstract;

  // Copies an Abstract already supplied under the Unknown contract. Explicit
  // construction makes the caller's interpretation visible. Construction
  // performs a record copy, with the policy guaranteed by its provider.
  explicit constexpr Unknown(Api api) : api(api) {}

  constexpr auto get_abstract() const -> Abstract { return api; }

  static EXPORTED(TTX) auto get_unknown() -> Unknown;

 private:
  Api api;
};

}  // namespace Ttx::Graph::Policies
