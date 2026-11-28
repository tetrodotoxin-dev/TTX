// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/none.hpp"

#include "ttx/graph/policies/constant.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Graph;

// A built in provider that satisfies the None contract in C++.
//
// Since this is an axiomatic perfect simulacra we can provide a single static
// instance to serve as a reference that any C++ or C implementation can return
// without being required to allocate a new instance. This also makes recursive
// navigation memory safe since all routes point to itself.
//
// Consumers should not assume this provider is the only provider of the None
// contract. Using the memory address of the provider is not a stand in for
// checking if an Abstract supports None.
class NoneProvider {
 public:
  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    const Identity::Uuid contract(id);
    if (contract == Identity::Uuid(TTX_UNKNOWN_ID_HIGH, TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    return contract == Abstract::contract_id ||
                   contract == Policies::None::contract_id ||
                   contract == Policies::Constant::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    if (Identity::Uuid(id) != Abstract::contract_id) {
      return TTX_NEGOTIATION_REJECTED;
    }

    *Data::cast<ttx_abstract>(output) = get_abstract(context);
    return TTX_NEGOTIATION_SATISFIED;
  }

  static auto get_abstract(void* context) -> ttx_abstract {
    return ttx_abstract(context, &operations);
  }

  static auto route(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    return get_abstract(context);
  }

  static auto visit(void* context, ttx_graph_visitor callback) -> void {}

  static auto get_reachability(void* context) -> ttx_abstract_reachability {
    return TTX_ABSTRACT_UNREACHABLE;
  }

  static constexpr ttx_abstract_ops operations = {
    supports, bind, route, visit, get_reachability};
};

auto Policies::None::get_none() -> None {
  static NoneProvider provider;
  return None(NoneProvider::get_abstract(&provider));
}

C_LINKAGE auto ttx_none(void) -> ttx_abstract {
  return Policies::None::get_none().get_abi();
}
