// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/ordered.h"

namespace Ttx::Graph::Policies {

// Ordered carries the Abstract whose visitation order has semantic meaning.
// The consumer preserves repeated occurrences and their positions. Unknown
// leaves the ordering promise undetermined, while Rejected establishes that
// visitation order is incidental under every policy of that Abstract.
class Ordered {
 public:
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_ORDERED_ID_HIGH,
      TTX_ORDERED_ID_LOW);
  using Api = ttx_abstract;

  // Copies an Abstract already supplied under the Ordered contract. Explicit
  // construction makes the caller's interpretation visible. Construction
  // performs a record copy, with the policy guaranteed by its provider.
  explicit constexpr Ordered(Api api) : api(api) {}

  // Checks the required pointers through `Abstract::accept`. The caller must
  // supply a readable table with the agreed representation. The provider
  // guarantees the Ordered policy itself.
  static auto accept(Api api) -> Bool { return Abstract::accept(api); }

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract { return Abstract(api); }

 private:
  Api api;
};

}  // namespace Ttx::Graph::Policies
