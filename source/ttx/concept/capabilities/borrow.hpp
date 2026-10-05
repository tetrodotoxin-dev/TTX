// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/capabilities/borrow.h"
#include "ttx/concept/policies/borrowed.hpp"

namespace Ttx::Concept::Capabilities {

// Exposes the capability to keep an Abstract available beyond the observation
// that supplied it. The provider can retain the current object or supply
// another object that preserves the encountered policy.
//
// `borrow` returns a Result containing the Borrowed answer when access can be
// retained, or an Unknown or Rejected failure. Unknown leaves the borrowing
// request undetermined. Rejected explicitly refuses it. Each successful call
// creates one release obligation, even when several calls return the same
// context. The caller fulfills that obligation by explicitly calling `release`
// on the returned Borrowed view.
//
// Returning Satisfied promises an object pointer and the full Borrowed
// interface, including its Abstract operations. Missing required pointers
// violates that contract, so the C++ adapter reports Rejected. If the result
// still contains an object pointer and a release operation, the adapter first
// calls `release` with that pointer to return the acquired access. The caller
// receives the failure after this cleanup.
class Borrow {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_BORROW_ID_HIGH, TTX_BORROW_ID_LOW);
  using Api = ttx_borrow;
  using Operations = ttx_borrow_ops;

  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->borrow &&
           Abstract::accept(
               ttx_abstract(api.context, &api.operations->abstract));
  }

  explicit constexpr Borrow(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract {
    return Abstract(api.context, api.operations->abstract);
  }

  auto borrow() const -> Perimortem::Utility::
      Result<Policies::Borrowed, Semantic::Negotiation::Binding::Failure>;

 private:
  Api api;
};

}  // namespace Ttx::Concept::Capabilities

TTX_DATA_RECORD(
    ttx_borrow_ops,
    TTX_DATA_MEMBER(ttx_borrow_ops, abstract),
    TTX_DATA_MEMBER(ttx_borrow_ops, borrow));
TTX_DATA_RECORD(
    ttx_borrow,
    TTX_DATA_MEMBER(ttx_borrow, context),
    TTX_DATA_MEMBER(ttx_borrow, operations));
