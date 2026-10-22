// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/constant.h"

namespace Ttx::Graph::Policies {

// Constant lets a consumer reuse a graph selection. A provider supplying
// Constant must preserve the graph location selected by the supplying Abstract
// and route throughout the established access duration. Repeating that route
// request must select the same position. Returning through `get_abstract` must
// preserve that position even when a binding produces a different API record.
// The selected node's data and outgoing observations can still change under
// their own contracts.
class Constant {
 public:
  static constexpr auto contract_id = Perimortem::Core::Identity::Uuid(
      TTX_CONSTANT_ID_HIGH,
      TTX_CONSTANT_ID_LOW);
  using Api = ttx_abstract;

  // Copies an Abstract already supplied under the Constant contract. Explicit
  // construction makes the caller's interpretation visible. Construction
  // performs a record copy, with the policy guaranteed by its provider.
  explicit constexpr Constant(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract { return Abstract(api); }

 private:
  Api api;
};

}  // namespace Ttx::Graph::Policies
