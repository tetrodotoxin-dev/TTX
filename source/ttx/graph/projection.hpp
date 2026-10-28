// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/data.hpp"
#include "perimortem/core/identity/uuid.hpp"
#include "perimortem/core/storage/option.hpp"
#include "perimortem/core/view/bytes.hpp"

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/none.h"
#include "ttx/semantic/negotiation/status.hpp"

namespace Ttx::Graph {

// Projection publishes one complete native C++ value through an Abstract.
// Concrete values provide the bindings specific to their domain. Projection
// provides the native contract identity, typed wide pointer, graph reentry and
// Abstract operation table that every native provider otherwise repeats.
//
// A projected value remains an ordinary C++ object. Its provider controls its
// storage and construction. Static values can be initialized during
// compilation, while dynamic providers can materialize complete values for a
// declared access window. Another C++ provider built for the same Concrete ABI
// can publish the native contract with another context and operation table.
// Consumers therefore call the functions supplied in each record and never
// infer that an unfamiliar context points to Concrete. Providers using another
// native type system publish their interoperable domain contracts instead.
//
// ProjectionHigh and ProjectionLow identify the generated native projection
// record. That UUID belongs to Abstract below rather than to Concrete's domain.
// It identifies access to Concrete's complete C++ ABI and changes when that
// native interface becomes incompatible. Portable domain contracts retain
// their own UUIDs and representations.
// A `supported_contracts` range adds fixed promises reported by `supports`.
// `supports_contract` can report promises selected from dynamic state. Neither
// mechanism requires the provider to supply an API record. Projection always
// binds its native Abstract and Graph::Abstract. `bind_contract` handles every
// remaining bind request when Concrete provides it. Route resolution and
// visitation follow the same extension pattern.
template <typename Concrete, U64 ProjectionHigh, U64 ProjectionLow>
class Projection {
 public:
  // Abstract is the typed wide pointer for one Concrete projection. Its context
  // is meaningful only to the supplied operations. `get_typed` returns the
  // complete native value, while `get_abstract` returns the graph location that
  // supplied the binding. A foreign implementation can use distinct contexts
  // for those two interfaces.
  //
  // Default construction creates an empty record for containers and refused
  // conversions. Calling either access operation on an empty record is
  // undefined behavior. The Bool conversion checks the complete record before
  // use.
  class Abstract {
   public:
    static constexpr auto contract_id =
        Perimortem::Core::Identity::Uuid(ProjectionHigh, ProjectionLow);

    struct Operations {
      constexpr Operations(
          const void* (*get_typed)(void*),
          ttx_abstract (*get_abstract)(void*))
          : get_typed(get_typed), get_abstract(get_abstract) {}

      const void* (*get_typed)(void* context);
      ttx_abstract (*get_abstract)(void* context);
    };

    struct Api {
      constexpr Api() = default;
      constexpr Api(void* context, const Operations& operations)
          : context(context), operations(&operations) {}

      void* context = nullptr;
      const Operations* operations = nullptr;
    };

    constexpr Abstract() = default;
    explicit constexpr Abstract(Api api) : api(api) {}

    constexpr auto get_abi() const -> Api { return api; }

    constexpr explicit operator Bool() const {
      return api.context != nullptr && api.operations != nullptr;
    }

    auto get_typed() const -> const Concrete& {
      return *Perimortem::Core::Data::cast<Concrete>(
          api.operations->get_typed(api.context));
    }

    auto get_abstract() const -> Graph::Abstract {
      return Graph::Abstract(api.operations->get_abstract(api.context));
    }

    // Conversion requests Concrete's complete typed record from the supplied
    // graph location. An undetermined or refused binding returns an empty
    // Option. The provider defines the returned value's access duration.
    static auto convert(Graph::Abstract abstract)
        -> Perimortem::Core::Storage::Option<Abstract> {
      Api output;
      const ttx_abstract graph = abstract.get_abi();
      const auto status =
          graph.operations->bind(graph.context, contract_id, &output);
      if (status == TTX_NEGOTIATION_SATISFIED) {
        return Abstract(output);
      }

      return {};
    }

   private:
    Api api;
  };

  // Publishes this complete Concrete value as one Present graph location. The
  // returned record keeps no ownership. Concrete's provider establishes the
  // access duration for the value and generated operation tables.
  auto get_abstract() -> Graph::Abstract {
    return project(*Perimortem::Core::Data::cast<Concrete>(this));
  }

 private:
  using Status = Semantic::Negotiation::Status;
  using Uuid = Perimortem::Core::Identity::Uuid;

  static auto concrete(void* context) -> Concrete& {
    return *Perimortem::Core::Data::cast<Concrete>(context);
  }

  static auto project(Concrete& value) -> Graph::Abstract {
    return Graph::Abstract(&value, graph_operations);
  }

  static auto typed(void* context) -> const void* {
    return Perimortem::Core::Data::cast<Concrete>(context);
  }

  static auto reenter(void* context) -> ttx_abstract {
    return project(concrete(context)).get_abi();
  }

  static constexpr auto reserved(Uuid contract) -> Bool {
    const perimortem_uuid value = contract;
    return value.high == 0 && value.low <= 1;
  }

  static constexpr auto advertised(Uuid contract) -> Bool {
    if constexpr (requires { Concrete::supported_contracts.get_size(); }) {
      for (Count index = 0; index < Concrete::supported_contracts.get_size();
           ++index) {
        if (Concrete::supported_contracts[index] == contract) {
          return True;
        }
      }
    }

    return False;
  }

  static auto bind_extended(Concrete& value, Uuid contract, void* output)
      -> ttx_negotiation_status {
    if constexpr (requires { value.bind_contract(contract, output); }) {
      return ttx_negotiation_status(value.bind_contract(contract, output));
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto support_extended(Concrete& value, Uuid contract)
      -> ttx_negotiation_status {
    if constexpr (requires { value.supports_contract(contract); }) {
      return ttx_negotiation_status(value.supports_contract(contract));
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto resolve(Concrete& value, Perimortem::Core::View::Bytes route)
      -> ttx_abstract {
    if constexpr (requires { value.resolve_route(route).get_abi(); }) {
      return value.resolve_route(route).get_abi();
    }

    return ttx_none();
  }

  static auto visit(Concrete& value, Graph::Abstract::Visitor visitor) -> void {
    if constexpr (requires { value.visit_routes(visitor); }) {
      value.visit_routes(visitor);
    }
  }

  static auto supports(void* context, perimortem_uuid requested)
      -> ttx_negotiation_status {
    const Uuid contract(requested);
    if (reserved(contract)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    if (contract == Abstract::contract_id ||
        contract == Graph::Abstract::contract_id || advertised(contract)) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    return support_extended(concrete(context), contract);
  }

  static auto bind(void* context, perimortem_uuid requested, void* output)
      -> ttx_negotiation_status {
    const Uuid contract(requested);
    if (reserved(contract)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    if (contract == Abstract::contract_id) {
      const typename Abstract::Api api(context, typed_operations);
      *Perimortem::Core::Data::cast<typename Abstract::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (contract == Graph::Abstract::contract_id) {
      *Perimortem::Core::Data::cast<ttx_abstract>(output) = reenter(context);
      return TTX_NEGOTIATION_SATISFIED;
    }

    return bind_extended(concrete(context), contract, output);
  }

  static auto resolve_route(void* context, perimortem_view_bytes requested)
      -> ttx_abstract {
    return resolve(concrete(context), Perimortem::Core::View::Bytes(requested));
  }

  static auto visit_routes(void* context, ttx_graph_visitor visitor) -> void {
    auto callback = [&](Perimortem::Core::View::Bytes route,
                        Graph::Abstract abstract) {
      visitor.callback(visitor.callback_context, route, abstract.get_abi());
    };

    visit(concrete(context), Graph::Abstract::Visitor(callback));
  }

  static auto get_state(void* context) -> ttx_abstract_state {
    (void)context;
    return TTX_ABSTRACT_PRESENT;
  }

  inline static constexpr typename Abstract::Operations typed_operations =
      typename Abstract::Operations(typed, reenter);

  inline static constexpr ttx_abstract_ops graph_operations = {
    supports, bind, resolve_route, visit_routes, get_state,
  };
};

}  // namespace Ttx::Graph
