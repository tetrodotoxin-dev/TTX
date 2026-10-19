// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/policies/none.h"

namespace Ttx::Concept::Policies {

// None carries the governing Abstract for a rejected route. The shared
// implementation resolves its own routes to itself. The original Abstract's
// selection can change unless it also supplies Constant for that route.
class None {
 public:
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_NONE_ID_HIGH,
      TTX_NONE_ID_LOW);
  using Api = ttx_abstract;

  // Copies an Abstract already supplied under the None contract. Explicit
  // construction makes the caller's interpretation visible. Construction
  // performs a record copy, with the policy guaranteed by its provider.
  explicit constexpr None(Api api) : api(api) {}

  // Uses Abstract's pointer presence check on a readable, correctly represented
  // table. The supplying contract establishes the policy's semantic guarantee.
  static auto accept(Api api) -> Bool { return Abstract::accept(api); }

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract { return Abstract(api); }

  static EXPORTED(TTX) auto get_none() -> None;

 private:
  Api api;
};

}  // namespace Ttx::Concept::Policies
