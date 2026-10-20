// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/providers/abstract/provider.h"

#include "perimortem/core/view/null_terminated.hpp"

#include "toolchain/export.h"
#include "ttx/concept/abstract.hpp"
#include "ttx/concept/policies/unknown.h"
#include "ttx/data/transfer.hpp"
#include "ttx/semantic/negotiation/library.h"

using namespace Perimortem;
using namespace Ttx;
using namespace Ttx::Concept;

TTX_DATA_RECORD(
    bootstrap_exports,
    TTX_DATA_MEMBER(bootstrap_exports, context),
    TTX_DATA_MEMBER(bootstrap_exports, visit));
TTX_DATA_RECORD(
    bootstrap_indexed,
    TTX_DATA_MEMBER(bootstrap_indexed, context),
    TTX_DATA_MEMBER(bootstrap_indexed, count),
    TTX_DATA_MEMBER(bootstrap_indexed, get));

namespace {

struct Root {
  static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
    return Serialization::Formats::Uuid(id) == Abstract::contract_id ? TTX_BINDING_SATISFIED
                                                     : TTX_BINDING_UNKNOWN;
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations};
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage target)
      -> ttx_binding_status {
    if (supports(context, id) != TTX_BINDING_SATISFIED) {
      return TTX_BINDING_UNKNOWN;
    }

    auto api = resolve(context);
    return Data::Transfer::copy(
               Data::Form::Storage::from(api), Data::Form::Storage(target)) ==
                   Data::Status::Success
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  static auto data(void*) -> perimortem_view_bytes {
    const auto bytes = "hello"_view;
    return {bytes.get_data(), bytes.get_size()};
  }

  static auto lookup(void*, perimortem_view_bytes) -> ttx_abstract {
    return ttx_unknown();
  }

  static void visit(void*, ttx_concept_visitor) {}

  static constexpr ttx_abstract_ops operations = {supports, bind,   data,
                                                  resolve,  lookup, visit};
};

struct Module {
  U64 requests = 0;
  Root root;
};

Module module;

void visit(
    void* context,
    void* callback_context,
    void (*callback)(void*, ttx_abstract)) {
  callback(callback_context, Root::resolve(context));
}

U64 count(void*) {
  return 1;
}

ttx_abstract get(void* context, U64 index) {
  return index == 0 ? Root::resolve(context) : ttx_unknown();
}

ttx_binding_status supports(void*, perimortem_uuid id) {
  const Serialization::Formats::Uuid contract(id);
  return contract == Abstract::contract_id ||
                 contract ==
                     Serialization::Formats::Uuid(
                         BOOTSTRAP_EXPORTS_HIGH, BOOTSTRAP_EXPORTS_LOW) ||
                 contract ==
                     Serialization::Formats::Uuid(BOOTSTRAP_QUERY_HIGH, BOOTSTRAP_QUERY_LOW)
             ? TTX_BINDING_SATISFIED
             : TTX_BINDING_UNKNOWN;
}

// Discovery uses the Module context, while each returned API uses its Root.
// Consumers call the returned functions with that Root context rather than
// reusing the discovery context. The functions remain hidden from symbol
// lookup and callable through their API records while the library is loaded.
ttx_binding_status bind(void* context, perimortem_uuid id, ttx_storage target) {
  auto& state = *static_cast<Module*>(context);
  ++state.requests;
  const Serialization::Formats::Uuid contract(id);
  if (contract == Abstract::contract_id) {
    return Root::bind(&state.root, id, target);
  }

  if (contract == Serialization::Formats::Uuid(BOOTSTRAP_QUERY_HIGH, BOOTSTRAP_QUERY_LOW)) {
    ttx_semantic_query api{&state.root, Root::bind, Root::supports};
    return Data::Transfer::copy(
               Data::Form::Storage::from(api), Data::Form::Storage(target)) ==
                   Data::Status::Success
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  if (contract == Serialization::Formats::Uuid(BOOTSTRAP_EXPORTS_HIGH, BOOTSTRAP_EXPORTS_LOW)) {
    bootstrap_exports api{&state.root, visit};
    if (Data::Transfer::copy(
            Data::Form::Storage::from(api), Data::Form::Storage(target)) ==
        Data::Status::Success) {
      return TTX_BINDING_SATISFIED;
    }

    // One export inventory supports callback and indexed consumer formats.
    // Both select the same root, with independent callable representations.
    bootstrap_indexed indexed{&state.root, count, get};
    return Data::Transfer::copy(
               Data::Form::Storage::from(indexed),
               Data::Form::Storage(target)) == Data::Status::Success
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  return TTX_BINDING_UNKNOWN;
}

ttx_semantic_query entry{&module, bind, supports};

}  // namespace

// The symbol is data. Its payload and canonical form remain available while
// the host keeps this module loaded. No provider function runs during import.
C_LINKAGE EXPORTED(TTX_TEST) const ttx_storage ttx_entry = {
  &Data::Form::Compiled<
      Data::Form::Native<ttx_semantic_query>::reference>::get_representation(),
  reinterpret_cast<U8*>(&entry), sizeof(entry)};
