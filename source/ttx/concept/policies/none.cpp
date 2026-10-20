// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/none.hpp"

#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/data/transfer.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Serialization;
using namespace Ttx::Concept;
using namespace Ttx::Data;

// NoneProvider supplies the shared None result. Its own routes stay on
// this result, while the original Abstract and route make a new selection on
// each observation unless they also supply Constant.
class NoneProvider {
 public:
  static auto supports(void*, perimortem_uuid id) -> ttx_negotiation_status {
    const Formats::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Policies::None::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage target)
      -> ttx_negotiation_status {
    if (supports(context, id) != TTX_NEGOTIATION_SATISFIED) {
      return TTX_NEGOTIATION_UNKNOWN;
    }

    Abstract::Api api = resolve(context);
    return Transfer::copy(Form::Storage::from(api), Form::Storage(target)) ==
                   Status::Success
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  static auto data(void*) -> perimortem_view_bytes {
    const auto bytes = "None"_view;
    return perimortem_view_bytes(bytes.get_data(), bytes.get_size());
  }

  static auto resolve(void* context) -> ttx_abstract {
    return ttx_abstract(context, &operations);
  }

  static auto route(void* context, perimortem_view_bytes) -> ttx_abstract {
    return resolve(context);
  }

  static auto visit(void*, ttx_concept_visitor) -> void {}

  static constexpr ttx_abstract_ops operations = {supports, bind,  data,
                                                  resolve,  route, visit};
};

auto Policies::None::get_none() -> None {
  static NoneProvider provider;
  return None(NoneProvider::resolve(&provider));
}

C_LINKAGE auto ttx_none(void) -> ttx_abstract {
  return Policies::None::get_none().get_abi();
}
