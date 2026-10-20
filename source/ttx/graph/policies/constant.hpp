// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/constant.h"

namespace Ttx::Graph::Policies {

// Constant promises that the Abstract and route which supplied this result
// continue to select the same semantic value. The selected value's outgoing
// routes and byte observations establish their own promises independently.
//
// The governing Abstract preserves this policy through further negotiation.
// Storage and executable code remain available through the supplying access
// contract. An acquired Retained result carries its separate release
// obligation.
class Constant {
 public:
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_CONSTANT_ID_HIGH,
      TTX_CONSTANT_ID_LOW);
  using Api = ttx_abstract;

  // Copies an Abstract already supplied under the Constant contract. Explicit
  // construction makes the caller's interpretation visible. Construction
  // performs a record copy, with the policy guaranteed by its provider.
  explicit constexpr Constant(Api api) : api(api) {}

  // Checks the required pointers through `Abstract::accept`. The caller must
  // supply a readable table with the agreed representation. The provider
  // guarantees the Constant policy itself.
  static auto accept(Api api) -> Bool { return Abstract::accept(api); }

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract { return Abstract(api); }

 private:
  Api api;
};

}  // namespace Ttx::Graph::Policies
