// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/unknown.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Graph;

// A built in provider that satisfies the Unknown contract in C++.
//
// Since this is an axiomatic perfect simulacra we can provide a single static
// instance to serve as a reference that any C++ or C implementation can return
// without being required to allocate a new instance. This also makes recursive
// navigation memory safe since all routes point to itself.
//
// Consumers should not assume this provider is the only provider of the Unknown
// contract. Using the memory address of the provider is not a stand in for
// checking if an Abstract supports Unknown.
class UnknownProvider {
 public:
  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    const Identity::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Policies::Unknown::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    if (Identity::Uuid(id) != Abstract::contract_id) {
      return TTX_NEGOTIATION_UNKNOWN;
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
    return TTX_ABSTRACT_INDETERMINATE;
  }

  static constexpr ttx_abstract_ops operations = {
    supports, bind, route, visit, get_reachability};
};

auto Policies::Unknown::get_unknown() -> Unknown {
  static UnknownProvider provider;
  return Unknown(UnknownProvider::get_abstract(&provider));
}

C_LINKAGE auto ttx_unknown(void) -> ttx_abstract {
  return Policies::Unknown::get_unknown().get_abi();
}
