// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/policies/unknown.h"

namespace Ttx::Concept::Policies {

// Unknown carries the governing Abstract for an undetermined route result.
// Its C++ interface preserves the supplied context and operations. To retry
// resolution, the consumer uses the original Abstract and route.
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

  // Uses Abstract's pointer presence check on a readable, correctly represented
  // table. The supplying contract establishes the policy's semantic guarantee.
  static auto accept(Api api) -> Bool { return Abstract::accept(api); }

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract { return Abstract(api); }

  static EXPORTED(TTX) auto get_unknown() -> Unknown;

 private:
  Api api;
};

}  // namespace Ttx::Concept::Policies
