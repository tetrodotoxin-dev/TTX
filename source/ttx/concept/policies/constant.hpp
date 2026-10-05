// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/policies/constant.h"

namespace Ttx::Concept::Policies {

// Constant promises the same answer for this provider and semantic question
// throughout the graph lifetime. A consumer can reuse that answer while its
// supplying data remains available. Each outgoing question has its own promise.
//
// Borrowing keeps the data available. When an acquired answer also provides
// Constant, its release obligation remains until that borrow is returned.
class Constant {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_CONSTANT_ID_HIGH, TTX_CONSTANT_ID_LOW);
  using Api = ttx_abstract;

  explicit constexpr Constant(Api api) : api(api) {}

  static auto accept(Api api) -> Bool { return Abstract::accept(api); }

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract { return Abstract(api); }

 private:
  Api api;
};

}  // namespace Ttx::Concept::Policies
