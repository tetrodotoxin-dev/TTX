// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/none.hpp"

#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/data/transfer.hpp"

using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;
using namespace Perimortem;

namespace {

// This shared result has its own context. Its stability does not establish a
// Constant promise for the question that selected it.
class NoneProvider {
 public:
  static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
    const Serialization::Formats::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Policies::None::contract_id
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage target)
      -> ttx_binding_status {
    if (supports(context, id) != TTX_BINDING_SATISFIED) {
      return TTX_BINDING_UNKNOWN;
    }

    Abstract::Api api = resolve(context);
    return Ttx::Data::Transfer::copy(
               Ttx::Data::Form::Storage::from(api),
               Ttx::Data::Form::Storage(target)) == Ttx::Data::Status::Success
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
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

  static void visit(void*, ttx_concept_visitor) {}

  static constexpr ttx_abstract_ops operations = {supports, bind,  data,
                                                  resolve,  route, visit};
};

}  // namespace

auto Policies::None::get_none() -> None {
  static NoneProvider provider;
  return None(NoneProvider::resolve(&provider));
}

C_LINKAGE ttx_abstract ttx_none(void) {
  return Policies::None::get_none().get_abi();
}
