// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/unknown.hpp"

#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/data/transfer.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Serialization;
using namespace Ttx::Concept;
using namespace Ttx::Data;

// UnknownProvider supplies the shared Unknown result. Its own routes stay on
// this result, while the original Abstract and route make a new selection on
// each observation unless they also supply Constant.
class UnknownProvider {
 public:
  static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
    const Formats::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Policies::Unknown::contract_id
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage target)
      -> ttx_binding_status {
    if (supports(context, id) != TTX_BINDING_SATISFIED) {
      return TTX_BINDING_UNKNOWN;
    }

    Abstract::Api api = resolve(context);
    return Transfer::copy(Form::Storage::from(api), Form::Storage(target)) ==
                   Status::Success
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  static auto data(void*) -> perimortem_view_bytes {
    const auto bytes = "Unknown"_view;
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

auto Policies::Unknown::get_unknown() -> Unknown {
  static UnknownProvider provider;
  return Unknown(UnknownProvider::resolve(&provider));
}

C_LINKAGE auto ttx_unknown(void) -> ttx_abstract {
  return Policies::Unknown::get_unknown().get_abi();
}
