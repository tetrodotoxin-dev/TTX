// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/policies/ordered.h"

namespace Ttx::Concept::Policies {

// Ordered qualifies the route visitation behavior is both stable and ordered on
// repeat visits and that any changes are meaningful changes.
//
// Ordered negotiation returning `Unknown` does not mean the data isn't stable
// and ordered but it means that it's not guaranteed by this policy. Explicitly
// rejecting this policy should promise that no other policy provided by the
// abstract could be used to infer ordering and that consumers are allowed to
// assume that no meaningful information is provided by ordering, even if its
// stable for every subsequent visit.
class Ordered : public Abstract {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_ORDERED_ID_HIGH, TTX_ORDERED_ID_LOW);
  using Api = ttx_abstract;
  using Abstract::Abstract;
};

}  // namespace Ttx::Concept::Policies
