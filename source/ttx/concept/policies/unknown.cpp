// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/unknown.hpp"

#include "perimortem/core/null_terminated.hpp"

using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;
using namespace Perimortem;

namespace {

// This shared answer has its own context. Its stability does not establish a
// Constant promise for the question that selected it.
class UnknownProvider {
 public:
  static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
    const System::Uuid contract(id);
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

    return static_cast<ttx_binding_status>(Binding::provide<Abstract>(
        resolve(context), Ttx::Data::Form::Storage(target)));
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

  static void visit(void*, ttx_concept_visitor) {}

  static constexpr ttx_abstract_ops operations = {supports, bind,  data,
                                                  resolve,  route, visit};
};

}  // namespace

auto Policies::Unknown::get_unknown() -> Unknown {
  static UnknownProvider provider;
  return Unknown(UnknownProvider::resolve(&provider));
}

C_LINKAGE ttx_abstract ttx_unknown(void) {
  return Policies::Unknown::get_unknown().get_abi();
}
