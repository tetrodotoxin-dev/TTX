// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/null_terminated.hpp"

#include "toolchain/export.h"
#include "ttx/concept/abstract.hpp"
#include "ttx/concept/policies/unknown.h"
#include "ttx/semantic/negotiation/library.h"

using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

namespace {

// This representation stays private to the loaded module. The consumer sees
// only the record supplied by the entry point.
struct Provider {
  static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
    return Perimortem::System::Uuid(id) == Abstract::contract_id
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_binding_status {
    if (supports(context, id) != TTX_BINDING_SATISFIED) {
      return TTX_BINDING_UNKNOWN;
    }

    return static_cast<ttx_binding_status>(Binding::provide<Abstract>(
        resolve(context), Ttx::Data::Form::Storage(output)));
  }

  static auto data(void*) -> perimortem_view_bytes {
    const auto bytes = "subject"_view;
    return {bytes.get_data(), bytes.get_size()};
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations};
  }

  static auto lookup(void*, perimortem_view_bytes) -> ttx_abstract {
    return ttx_unknown();
  }

  static void visit(void*, ttx_concept_visitor) {}

  static constexpr ttx_abstract_ops operations = {supports, bind,   data,
                                                  resolve,  lookup, visit};
};

}  // namespace

C_LINKAGE EXPORTED(TTX_TEST)
ttx_abstract abstract_subject() {
  static Provider provider;
  return Provider::resolve(&provider);
}

// The callback borrows this provider for one observation.
C_LINKAGE EXPORTED(TTX_TEST)
ttx_binding_status ttx_query(
    ttx_semantic_query host,
    ttx_query_callback callback) {
  const perimortem_uuid admission(0x5ef6275871544cf7ULL, 0xb3ee8d4b319b4b75ULL);
  const auto status = host.supports(host.context, admission);
  if (status != TTX_BINDING_SATISFIED) {
    return status;
  }

  Provider provider;
  return callback.callback(
      callback.context, {&provider, Provider::bind, Provider::supports});
}
