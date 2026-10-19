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
// `borrow` returns a Result containing the Borrowed result when access can be
// retained, or an Unknown or Rejected status. Unknown leaves the borrowing
// request undetermined. Rejected explicitly refuses it. Each successful call
// creates one release obligation, even when several calls return the same
// context. The caller fulfills that obligation by explicitly calling `release`
// on the returned Borrowed view.
//
// Returning Satisfied promises a nonnull context and the full Borrowed
// interface, including its Abstract operations. Missing required pointers
// violates that contract, so the C++ adapter reports Rejected. If the result
// still contains a nonnull context and a release operation, the adapter first
// calls `release` with that context to return the acquired access. The caller
// receives Rejected after this cleanup.
class Borrow {
 public:
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_BORROW_ID_HIGH,
      TTX_BORROW_ID_LOW);
  using Api = ttx_borrow;
  using Operations = ttx_borrow_ops;

  // Checks `borrow`, context and the required Abstract operations for null.
  // Any present table must be readable and match the established API
  // representation. The supplying contract establishes Borrow's meaning.
  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->borrow &&
           Abstract::accept(
               ttx_abstract(api.context, &api.operations->abstract));
  }

  // Copies a record already supplied under the Borrow contract. The caller
  // establishes its representation and required pointers. Explicit construction
  // marks this unchecked conversion from the C record.
  explicit constexpr Borrow(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract {
    return Abstract(api.context, api.operations->abstract);
  }

  EXPORTED(TTX) auto borrow() const -> Perimortem::Core::Storage::
      Result<Policies::Borrowed, Semantic::Negotiation::Binding::Status>;

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
