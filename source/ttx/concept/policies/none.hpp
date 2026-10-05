// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/policies/none.h"

namespace Ttx::Concept::Policies {

// None is the Abstract answer for a rejected route. The shared node returns
// itself for every route, but that says nothing about whether the originating
// provider will reject a later observation. Combining None with Constant makes
// that stronger promise for the particular edge.
class None {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_NONE_ID_HIGH, TTX_NONE_ID_LOW);
  using Api = ttx_abstract;

  explicit constexpr None(Api api) : api(api) {}

  static auto accept(Api api) -> Bool { return Abstract::accept(api); }

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract { return Abstract(api); }

  static auto get_none() -> None;

 private:
  Api api;
};

}  // namespace Ttx::Concept::Policies
