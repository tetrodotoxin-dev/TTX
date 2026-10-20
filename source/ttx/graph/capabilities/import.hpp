// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/data.hpp"

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/capabilities/import.h"

namespace Ttx::Graph::Capabilities {

// Import interprets described data as a graph of Abstracts. The provider checks
// the Representation before interpreting input and supplies the resulting
// Abstract through one synchronous callback. A successful import returns
// Satisfied after that callback finishes.
//
// The callback is a callable passed by reference and invoked with an Abstract.
// Its captured state carries the consumer's observations. The call returns the
// provider's status and discards any value returned by the callback. The
// input, Representation and callback remain available until the call returns.
// The caller supplies a readable region with the represented extent and
// alignment. The wrapper forwards these arguments to the provider directly.
// The imported graph is available during the callback, where the consumer can
// inspect it, copy data or negotiate retained access through the graph's
// capabilities.
//
// A `visit` returning Unknown or Rejected guarantees that the callback was
// never invoked. Unknown leaves the import request undetermined. Rejected
// explicitly refuses it.
class Import {
 public:
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_IMPORT_ID_HIGH,
      TTX_IMPORT_ID_LOW);
  using Api = ttx_import;
  using Operations = ttx_import_ops;

  // Checks `visit`, context and the required Abstract operations for null.
  // Any present table must be readable and match the established API
  // representation. The supplying contract establishes Import's meaning.
  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->visit &&
           Abstract::accept(
               ttx_abstract(api.context, &api.operations->abstract));
  }

  // Copies a record already supplied under the Import contract. The caller
  // establishes its representation and required pointers. Explicit construction
  // marks this unchecked conversion from the C record.
  explicit constexpr Import(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract {
    return Abstract(api.context, api.operations->abstract);
  }

  template <typename Function>
  auto visit(
      const void* input,
      const Data::Form::Representation& representation,
      Function& callback) const -> Semantic::Negotiation::Status {
    const auto api = get_abi();
    return Semantic::Negotiation::Status(api.operations->visit(
        api.context, input, &representation.get_abi(), &callback,
        Import::callback<Function>));
  }

 private:
  template <typename Function>
  static auto callback(void* context, ttx_abstract abstract) -> void {
    (*Perimortem::Core::Data::cast<Function>(context))(Abstract(abstract));
  }

  Api api;
};

}  // namespace Ttx::Graph::Capabilities

TTX_DATA_RECORD(
    ttx_import_ops,
    TTX_DATA_MEMBER(ttx_import_ops, abstract),
    TTX_DATA_MEMBER(ttx_import_ops, visit));
TTX_DATA_RECORD(
    ttx_import,
    TTX_DATA_MEMBER(ttx_import, context),
    TTX_DATA_MEMBER(ttx_import, operations));
