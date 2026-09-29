// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once
#include "ttx/concept/abstract.hpp"
#include "ttx/concept/capabilities/create.h"

namespace Ttx::Concept::Capabilities {

// Exposes the capability to create an Abstract from the supplied argument
// graph. The provider interprets the arguments through their routes and
// contracts. None represents intentionally absent arguments. If creation
// succeeds, create invokes the receiver once with the created Abstract and
// returns Satisfied after the callback finishes.
//
// The receiver is a callable passed by reference and invoked with an Abstract.
// Its captured state carries the consumer's observations. create returns the
// provider's status and discards any value returned by the receiver. The
// arguments and receiver remain available until create returns. The created
// Abstract is available during the callback. The consumer can use its
// capabilities, copy observations or negotiate retained access, allowing
// providers to create objects in temporary storage.
//
// A create call returning Unknown or Rejected guarantees that the receiver was
// never invoked. Unknown leaves the creation request undetermined. Rejected
// explicitly refuses it. provide adapts a native owner's create operation and
// preserves that owner's Abstract interface.
class Create : public Abstract {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_CREATE_ID_HIGH, TTX_CREATE_ID_LOW);
  using Api = ttx_create;
  using Operations = ttx_create_ops;
  static auto accept(Api value) -> Bool {
    return value.operations && value.operations->create &&
           Abstract::accept(
               ttx_abstract(value.source, &value.operations->abstract));
  }
  explicit constexpr Create(Api value)
      : Abstract(value.source, value.operations->abstract) {}
  constexpr auto get_abi() const -> Api {
    const auto value = Abstract::get_abi();
    return Api(
        value.source, reinterpret_cast<const Operations*>(value.operations));
  }

  template <typename Receiver>
  auto create(Abstract arguments, Receiver& receiver) const
      -> Semantic::Negotiation::Binding::Status {
    const auto api = get_abi();
    return static_cast<Semantic::Negotiation::Binding::Status>(
        api.operations->create(
            api.source, arguments.get_abi(), &receiver,
            [](void* state, ttx_abstract subject) {
              (*static_cast<Receiver*>(state))(Abstract(subject));
            }));
  }

  template <typename Owner>
    requires(!__is_base_of(Abstract, Owner))
  static auto provide(const Owner& owner) -> Create {
    static const Operations operations = Operations(
        *Abstract::provide(owner).get_abi().operations,
        [](const void* source, ttx_abstract arguments, void* receiver,
           void (*receive)(void*, ttx_abstract)) -> ttx_binding_status {
          auto observe = [&](Abstract subject) {
            receive(receiver, subject.get_abi());
          };
          return static_cast<ttx_binding_status>(
              static_cast<const Owner*>(source)->create(
                  Abstract(arguments), observe));
        });
    return Create(Api(&owner, &operations));
  }
};

}  // namespace Ttx::Concept::Capabilities

TTX_DATA_RECORD(
    ttx_create_ops,
    TTX_DATA_MEMBER(ttx_create_ops, abstract),
    TTX_DATA_MEMBER(ttx_create_ops, create));
TTX_DATA_RECORD(
    ttx_create,
    TTX_DATA_MEMBER(ttx_create, source),
    TTX_DATA_MEMBER(ttx_create, operations));
