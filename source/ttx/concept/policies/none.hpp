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
class None : public Abstract {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_NONE_ID_HIGH, TTX_NONE_ID_LOW);
  using Api = ttx_abstract;
  using Abstract::Abstract;
  static auto get_none() -> None;
};

}  // namespace Ttx::Concept::Policies
