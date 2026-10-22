// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/data.hpp"

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/capabilities/create.h"

namespace Ttx::Graph::Capabilities {

// Create constructs a result from an argument graph using the provider's
// interpretation of its routes and contracts. None represents intentionally
// absent arguments. A successful creation invokes the callback once with the
// created Abstract and returns Satisfied after the callback finishes.
//
// The callback is a callable passed by reference and invoked with an Abstract.
// Its captured state belongs to the consumer. The call returns the
// provider's status and discards any value returned by the callback. The
// arguments and callback remain available until the call returns. The created
// Abstract is available during the callback. The consumer can use its
// capabilities, copy typed results or negotiate retained access, allowing
// providers to create objects in temporary storage.
//
// A `create` call returning Unknown or Rejected guarantees that the callback
// was never invoked. Unknown leaves the creation request undetermined. Rejected
// explicitly refuses it.
class Create {
 public:
  static constexpr auto contract_id =
      Perimortem::Core::Identity::Uuid(TTX_CREATE_ID_HIGH, TTX_CREATE_ID_LOW);
  using Api = ttx_create;
  using Operations = ttx_create_ops;

  // Copies the complete C record supplied under the Create contract.
  // Explicit construction preserves the provider's context and operations
  // as one API, with their supplying access duration.
  explicit constexpr Create(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  // Returns to the graph location that supplied this binding. The provider
  // supplies the complete Abstract and may use a different context and table.
  auto get_abstract() const -> Abstract {
    return Abstract(api.operations->get_abstract(api.context));
  }

  template <typename Function>
  auto create(Abstract arguments, Function& callback) const
      -> Semantic::Negotiation::Status {
    const auto api = get_abi();
    return Semantic::Negotiation::Status(api.operations->create(
        api.context, arguments.get_abi(), &callback,
        Create::callback<Function>));
  }

 private:
  template <typename Function>
  static auto callback(void* context, ttx_abstract abstract) -> void {
    (*Perimortem::Core::Data::cast<Function>(context))(Abstract(abstract));
  }

  Api api;
};

}  // namespace Ttx::Graph::Capabilities
