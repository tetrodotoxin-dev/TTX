// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/policies/borrowed.h"

namespace Ttx::Concept::Policies {

// Borrowed gives C++ consumers acquired access with an explicit release call.
// Copying the API record or binding another interface shares the existing
// acquisition. Borrowing again creates a separate acquisition and obligation.
// The consumer calls release once for each acquired result after its last use.
// Using that acquisition or its copies after release is undefined behavior.
class Borrowed {
 public:
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_BORROWED_ID_HIGH,
      TTX_BORROWED_ID_LOW);
  using Api = ttx_borrowed;
  using Operations = ttx_borrowed_ops;

  // Checks release, context and all Abstract operation pointers for null. Any
  // present table must be readable and match the negotiated representation.
  // The supplying operation establishes the acquisition and release obligation.
  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->release &&
           Abstract::accept(
               ttx_abstract(api.context, &api.operations->abstract));
  }

  // Copies an existing Borrowed record without acquiring again. Explicit
  // construction leaves validation of the record with the caller. The
  // original release obligation remains unchanged.
  explicit constexpr Borrowed(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract {
    return Abstract(api.context, api.operations->abstract);
  }

  auto release() const -> void {
    const auto api = get_abi();
    api.operations->release(api.context);
  }

 private:
  Api api;
};

}  // namespace Ttx::Concept::Policies

TTX_DATA_RECORD(
    ttx_borrowed_ops,
    TTX_DATA_MEMBER(ttx_borrowed_ops, abstract),
    TTX_DATA_MEMBER(ttx_borrowed_ops, release));
TTX_DATA_RECORD(
    ttx_borrowed,
    TTX_DATA_MEMBER(ttx_borrowed, context),
    TTX_DATA_MEMBER(ttx_borrowed, operations));
