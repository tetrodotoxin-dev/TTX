// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/abi/operations.hpp"
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
// receiver. The caller fulfills that obligation by explicitly calling `release`
// on the returned Borrowed view.
//
// Returning Satisfied promises an object pointer and the full Borrowed
// interface, including its Abstract operations. Missing required pointers
// violates that contract, so the C++ adapter reports Rejected. If the result
// still contains an object pointer and a release operation, the adapter first
// calls `release` with that pointer to return the acquired access. The caller
// receives the failure after this cleanup.
class Borrow : public Abstract {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_BORROW_ID_HIGH, TTX_BORROW_ID_LOW);
  using Api = ttx_borrow;
  using Operations = ttx_borrow_ops;
  static auto accept(Api value) -> Bool {
    return value.operations && value.operations->borrow &&
           Abstract::accept(
               ttx_abstract(value.source, &value.operations->abstract));
  }
  explicit constexpr Borrow(Api value)
      : Abstract(value.source, value.operations->abstract) {}
  constexpr auto get_abi() const -> Api {
    const auto value = Abstract::get_abi();
    return Api(
        value.source, &Abi::Operations::from_abstract<Operations>(*value.operations));
  }
  auto borrow() const -> Perimortem::Utility::
      Result<Policies::Borrowed, Semantic::Negotiation::Binding::Failure>;

  template <typename Provider>
    requires(!__is_base_of(Abstract, Provider) && !__is_const(Provider))
  static auto provide(Provider& provider) -> Borrow {
    static const Operations operations = Operations(
        *Abstract::provide(provider).get_abi().operations,
        [](void* source, ttx_borrowed* output) -> ttx_binding_status {
          return Abi::Receiver::get<Provider>(source).borrow().visit(
              [&](Policies::Borrowed answer) -> ttx_binding_status {
                *output = answer.get_abi();
                return TTX_BINDING_SATISFIED;
              },
              [](Semantic::Negotiation::Binding::Failure failure) {
                return static_cast<ttx_binding_status>(failure);
              });
        });
    return Borrow(Api(&provider, &operations));
  }
};

}  // namespace Ttx::Concept::Capabilities

TTX_DATA_RECORD(
    ttx_borrow_ops,
    TTX_DATA_MEMBER(ttx_borrow_ops, abstract),
    TTX_DATA_MEMBER(ttx_borrow_ops, borrow));
TTX_DATA_RECORD(
    ttx_borrow,
    TTX_DATA_MEMBER(ttx_borrow, source),
    TTX_DATA_MEMBER(ttx_borrow, operations));
