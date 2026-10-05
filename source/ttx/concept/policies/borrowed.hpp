// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/policies/borrowed.h"

namespace Ttx::Concept::Policies {

// Borrowed exposes an Abstract whose acquired access lasts until release. An
// operation transferring retained access returns this contract and passes
// responsibility for the release to its caller. The provider keeps the
// answer's data and policy available until that release.
//
// Binding or copying a view of the answer shares the existing access and
// release obligation. Borrow requests another acquired answer, with its own
// release, even when the provider returns the same context again. The C++
// view leaves release explicit. The consumer of each acquired answer calls
// `release` once after using the retained answer.
//
// Operations that require the caller to release their result must return
// Borrowed or a stronger contract that requires it. Returning that result only
// as Abstract hides the release obligation and can leak the retained
// resources.
class Borrowed {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_BORROWED_ID_HIGH, TTX_BORROWED_ID_LOW);
  using Api = ttx_borrowed;
  using Operations = ttx_borrowed_ops;

  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->release &&
           Abstract::accept(
               ttx_abstract(api.context, &api.operations->abstract));
  }

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
