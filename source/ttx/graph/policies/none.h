// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_NONE_H
#define TTX_GRAPH_POLICIES_NONE_H

#include "ttx/graph/abstract.h"

// None reports that a selection has no target in this observation. A later
// lookup at the original location may return a different result. A caller
// can reuse a route selection when its provider independently grants Constant.
//
// An Abstract supplying None supports None and Abstract and reports
// Unreachable. Binding Abstract returns its own location. Binding None
// returns Rejected. Resolution and every route lead back to this location,
// visitation invokes zero callbacks, and these observations have no visible
// side effects. Additional policies carry their own obligations.
//
// Check support for None to recognize the policy on independently supplied
// records. The function `ttx_none()` returns the library's shared
// implementation, which declines Constant.
C_LINKAGE EXPORTED(TTX)
ttx_abstract ttx_none(void);

#ifdef __cplusplus

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

#endif

#endif
