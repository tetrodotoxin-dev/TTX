// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/policies/unknown.h"

namespace Ttx::Concept::Policies {

// Unknown is the provisional answer to a semantic question that the current
// graph cannot settle. Since repeating the original question may later produce
// a factual identity under a different observational state, Unknown can't be
// used to assume the resolution would never produce a valid answer.
//
// For explicit rejection None should be used instead.
class Unknown : public Abstract {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_UNKNOWN_ID_HIGH, TTX_UNKNOWN_ID_LOW);
  using Api = ttx_abstract;
  using Abstract::Abstract;
  static auto get_unknown() -> Unknown;
};

}  // namespace Ttx::Concept::Policies
