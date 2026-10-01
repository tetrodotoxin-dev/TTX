// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/abi/operations.hpp"
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
// release, even when the provider returns the same receiver again. The C++
// view leaves release explicit. The consumer of each acquired answer calls
// `release` once after using the retained answer.
//
// Operations that require the caller to release their result must return
// Borrowed or a stronger contract that requires it. Returning that result only
// as Abstract hides the release obligation and can leak the retained
// resources.
class Borrowed : public Abstract {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_BORROWED_ID_HIGH, TTX_BORROWED_ID_LOW);
  using Api = ttx_borrowed;
  using Operations = ttx_borrowed_ops;
  static auto accept(Api value) -> Bool {
    return value.operations && value.operations->release &&
           Abstract::accept(
               ttx_abstract(value.source, &value.operations->abstract));
  }
  explicit constexpr Borrowed(Api value)
      : Abstract(value.source, value.operations->abstract) {}
  constexpr auto get_abi() const -> Api {
    const auto value = Abstract::get_abi();
    return Api(
        value.source, &Abi::Operations::from_abstract<Operations>(*value.operations));
  }
  auto release() const -> void {
    const auto value = get_abi();
    value.operations->release(value.source);
  }
  template <typename Provider>
    requires(!__is_base_of(Abstract, Provider) && !__is_const(Provider))
  static auto provide(Provider& provider) -> Borrowed {
    static const Operations operations = Operations(
        *Abstract::provide(provider).get_abi().operations,
        [](void* source) {
          Abi::Receiver::get<Provider>(source).release();
        });
    return Borrowed(Api(&provider, &operations));
  }
};

}  // namespace Ttx::Concept::Policies

TTX_DATA_RECORD(
    ttx_borrowed_ops,
    TTX_DATA_MEMBER(ttx_borrowed_ops, abstract),
    TTX_DATA_MEMBER(ttx_borrowed_ops, release));
TTX_DATA_RECORD(
    ttx_borrowed,
    TTX_DATA_MEMBER(ttx_borrowed, source),
    TTX_DATA_MEMBER(ttx_borrowed, operations));
