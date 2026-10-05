// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/capabilities/create.h"

namespace Ttx::Concept::Capabilities {

// Exposes the capability to create an Abstract from the supplied argument
// graph. The provider interprets the arguments through their routes and
// contracts. None represents intentionally absent arguments. If creation
// succeeds, `create` invokes the callback once with the created Abstract and
// returns Satisfied after the callback finishes.
//
// The callback is a callable passed by reference and invoked with an Abstract.
// Its captured state carries the consumer's observations. `create` returns the
// provider's status and discards any value returned by the callback. The
// arguments and callback remain available until `create` returns. The created
// Abstract is available during the callback. The consumer can use its
// capabilities, copy observations or negotiate retained access, allowing
// providers to create objects in temporary storage.
//
// A `create` call returning Unknown or Rejected guarantees that the callback
// was never invoked. Unknown leaves the creation request undetermined. Rejected
// explicitly refuses it.
class Create {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_CREATE_ID_HIGH, TTX_CREATE_ID_LOW);
  using Api = ttx_create;
  using Operations = ttx_create_ops;

  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->create &&
           Abstract::accept(
               ttx_abstract(api.context, &api.operations->abstract));
  }

  explicit constexpr Create(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract {
    return Abstract(api.context, api.operations->abstract);
  }

  template <typename Function>
  auto create(Abstract arguments, Function& callback) const
      -> Semantic::Negotiation::Binding::Status {
    const auto api = get_abi();
    return static_cast<Semantic::Negotiation::Binding::Status>(
        api.operations->create(
            api.context, arguments.get_abi(), &callback,
            Create::callback<Function>));
  }

 private:
  template <typename Function>
  static void callback(void* context, ttx_abstract abstract) {
    (*static_cast<Function*>(context))(Abstract(abstract));
  }

  Api api;
};

}  // namespace Ttx::Concept::Capabilities

TTX_DATA_RECORD(
    ttx_create_ops,
    TTX_DATA_MEMBER(ttx_create_ops, abstract),
    TTX_DATA_MEMBER(ttx_create_ops, create));
TTX_DATA_RECORD(
    ttx_create,
    TTX_DATA_MEMBER(ttx_create, context),
    TTX_DATA_MEMBER(ttx_create, operations));
