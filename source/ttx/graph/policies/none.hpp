// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/none.h"

namespace Ttx::Graph::Policies {

// None represents a known durable dead end in the Abstract graph. It does not
// mark that an observation is empty, but instead signals that there is not and
// NEVER will be a location at the observed location in the graph: the requested
// location is permanently unreachable.
//
// The None contract represents one of the two axiomatic Abstracts and can only
// be reported as supported if the Abstract meets these exact policies:
//
// * It must also support both Constant and Abstract contracts.
// * It must explicitly reject all other contracts.
// * It must only satisfy bind for Abstract.
// * It must explicitly reject all other binds.
// * It must return Unreachable for it's reachability.
// * Every route from it must return itself.
// * Any observations on the Abstract produces no observable side effects.
//
// None's contract is the formal TTX domain identifier that provides a fast path
// for identification without needing to observe all behaviors. Any Abstract can
// emit support for the None contract must collapse to a perfect simulacra so
// TTX supports a built in Abstract that can be used to statically emit for ture
// unreachable paths. Consumers however should not assume that the static TTX
// address represents the one and only `None` representation.
class None {
 public:
  static constexpr auto contract_id =
      Perimortem::Core::Identity::Uuid(TTX_NONE_ID_HIGH, TTX_NONE_ID_LOW);
  using Api = ttx_abstract;

  // Copies an Abstract already supplied under the None contract. Explicit
  // construction makes the caller's interpretation visible. Construction
  // performs a record copy, with the policy guaranteed by its provider.
  explicit constexpr None(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract { return Abstract(api); }

  static EXPORTED(TTX) auto get_none() -> None;

 private:
  Api api;
};

}  // namespace Ttx::Graph::Policies
