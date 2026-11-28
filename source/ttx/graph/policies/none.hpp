// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/none.h"

namespace Ttx::Graph::Policies {

// None identifies an absent target for one lookup. The shared provider is
// available through `get_none()`. A reader inspecting an independent Abstract
// uses `supports<None>()` because the provider may publish a different record
// for the same absence policy.
class None {
 public:
  static constexpr auto contract_id =
      Perimortem::Core::Identity::Uuid(TTX_NONE_ID_HIGH, TTX_NONE_ID_LOW);
  using Api = ttx_abstract;

  // Copies an Abstract already supplied under the None contract. Explicit
  // construction makes the caller's interpretation visible. Construction
  // performs a record copy, with the policy guaranteed by its provider.
  explicit constexpr None(Api api) : api(api) {}

  constexpr auto get_abstract() const -> Abstract { return api; }

  static EXPORTED(TTX) auto get_none() -> None;

 private:
  Api api;
};

}  // namespace Ttx::Graph::Policies
