// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/providers/abstract/provider.h"

#include "toolchain/export.h"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/data/transfer.hpp"
#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/unknown.h"
#include "ttx/semantic/negotiation/library.h"

using namespace Perimortem::Core;
using namespace Perimortem::Serialization;
using namespace Ttx::Graph;
using namespace Ttx::Data;

TTX_DATA_RECORD(
    bootstrap_exports,
    TTX_DATA_MEMBER(bootstrap_exports, context),
    TTX_DATA_MEMBER(bootstrap_exports, visit));
TTX_DATA_RECORD(
    bootstrap_indexed,
    TTX_DATA_MEMBER(bootstrap_indexed, context),
    TTX_DATA_MEMBER(bootstrap_indexed, count),
    TTX_DATA_MEMBER(bootstrap_indexed, get));

struct Root {
  static auto state(void* context) -> ttx_abstract_state {
    return TTX_ABSTRACT_PRESENT;
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    return Formats::Uuid(id) == Abstract::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto get_abstract(void* context) -> ttx_abstract {
    return {context, &operations};
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

  static auto query_bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_negotiation_status {
    const auto status = supports(context, id);
    if (status != TTX_NEGOTIATION_SATISFIED) {
      return status;
    }

    auto api = get_abstract(context);
    return Transfer::copy(Form::Storage::from(api), Form::Storage(output)) ==
                   Status::Success
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    return ttx_unknown();
  }

  static auto visit(void* context, ttx_graph_visitor callback) -> void {}

  static constexpr ttx_abstract_ops operations = {
    supports, bind, lookup, visit, state};
};

struct Module {
  U64 requests = 0;
  Root root;
};

static Module module;

auto visit(
    void* context,
    void* callback_context,
    void (*callback)(void*, ttx_abstract)) -> void {
  callback(callback_context, Root::get_abstract(context));
}

auto count(void* context) -> U64 {
  return 1;
}

auto get(void* context, U64 index) -> ttx_abstract {
  return index == 0 ? Root::get_abstract(context) : ttx_unknown();
}

auto supports(void* context, perimortem_uuid id) -> ttx_negotiation_status {
  const Formats::Uuid contract(id);
  return contract == Abstract::contract_id ||
                 contract ==
                     Formats::Uuid(
                         BOOTSTRAP_EXPORTS_HIGH, BOOTSTRAP_EXPORTS_LOW) ||
                 contract ==
                     Formats::Uuid(BOOTSTRAP_QUERY_HIGH, BOOTSTRAP_QUERY_LOW)
             ? TTX_NEGOTIATION_SATISFIED
             : TTX_NEGOTIATION_UNKNOWN;
}

// Discovery uses the Module context, while each returned API uses its Root.
// Consumers call the returned functions with that Root context rather than
// reusing the discovery context. The functions remain hidden from symbol
// lookup and callable through their API records while the library is loaded.
auto bind(void* context, perimortem_uuid id, ttx_storage target)
    -> ttx_negotiation_status {
  auto& state = *Data::cast<Module>(context);
  ++state.requests;

  const Formats::Uuid contract(id);
  if (contract == Abstract::contract_id) {
    return Root::query_bind(&state.root, id, target);
  }

  if (contract == Formats::Uuid(BOOTSTRAP_QUERY_HIGH, BOOTSTRAP_QUERY_LOW)) {
    ttx_semantic_query api{&state.root, Root::query_bind, Root::supports};
    return Transfer::copy(Form::Storage::from(api), Form::Storage(target)) ==
                   Status::Success
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  if (contract ==
      Formats::Uuid(BOOTSTRAP_EXPORTS_HIGH, BOOTSTRAP_EXPORTS_LOW)) {
    bootstrap_exports api{&state.root, visit};
    if (Transfer::copy(Form::Storage::from(api), Form::Storage(target)) ==
        Status::Success) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    // One export inventory supports callback and indexed consumer formats.
    // Both select the same root, with independent callable representations.
    bootstrap_indexed indexed{&state.root, count, get};
    return Transfer::copy(
               Form::Storage::from(indexed), Form::Storage(target)) ==
                   Status::Success
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  return TTX_NEGOTIATION_UNKNOWN;
}

ttx_semantic_query entry{&module, bind, supports};

// The symbol is data. Its payload and canonical form remain available while
// the host keeps this module loaded. No provider function runs during import.
C_LINKAGE EXPORTED(TTX_TEST) const ttx_storage ttx_entry = {
  &Form::Compiled<
       Form::Native<ttx_semantic_query>::reference>::get_representation()
       .get_abi(),
  Data::cast<U8>(&entry), sizeof(entry)};
