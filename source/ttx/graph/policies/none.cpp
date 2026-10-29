// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/none.hpp"

#include "ttx/graph/policies/constant.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Graph;

// NoneProvider supplies the shared Constant dead end. Its own routes stay on
// this result, while the original Abstract and route make a new selection on
// each observation unless that selection establishes its own stability.
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
    const auto status = supports(context, id);
    if (status != TTX_NEGOTIATION_SATISFIED) {
      return status;
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

  static auto state(void* context) -> ttx_abstract_state {
    return TTX_ABSTRACT_NONE;
  }

  static constexpr ttx_abstract_ops operations = {
    supports, bind, route, visit, state};
};

auto Policies::None::get_none() -> None {
  static NoneProvider provider;
  return None(NoneProvider::get_abstract(&provider));
}

C_LINKAGE auto ttx_none(void) -> ttx_abstract {
  return Policies::None::get_none().get_abi();
}
