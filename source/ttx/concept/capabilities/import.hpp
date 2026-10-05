// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/capabilities/import.h"

namespace Ttx::Concept::Capabilities {

// Exposes the capability to interpret data described by the supplied
// Representation and supply an Abstract for the imported graph. The provider
// checks the representation before interpreting the input. On success, `visit`
// invokes the callback once with that Abstract and returns Satisfied after the
// callback finishes.
//
// The callback is a callable passed by reference and invoked with an Abstract.
// Its captured state carries the consumer's observations. `visit` returns the
// provider's status and discards any value returned by the callback. The
// input, Representation and callback remain available until `visit` returns.
// The imported graph is available during the callback, where the consumer can
// inspect it, copy data or negotiate retained access through the graph's
// capabilities.
//
// A `visit` returning Unknown or Rejected guarantees that the callback was
// never invoked. Unknown leaves the import request undetermined. Rejected
// explicitly refuses it.
class Import {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_IMPORT_ID_HIGH, TTX_IMPORT_ID_LOW);
  using Api = ttx_import;
  using Operations = ttx_import_ops;

  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->visit &&
           Abstract::accept(
               ttx_abstract(api.context, &api.operations->abstract));
  }

  explicit constexpr Import(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract {
    return Abstract(api.context, api.operations->abstract);
  }

  template <typename Function>
  auto visit(
      const void* input,
      const Data::Form::Representation& representation,
      Function& callback) const -> Semantic::Negotiation::Binding::Status {
    const auto api = get_abi();
    return static_cast<Semantic::Negotiation::Binding::Status>(
        api.operations->visit(
            api.context, input, &representation, &callback,
            Import::callback<Function>));
  }

 private:
  template <typename Function>
  static void callback(void* context, ttx_abstract abstract) {
    (*static_cast<Function*>(context))(Abstract(abstract));
  }

  Api api;
};

}  // namespace Ttx::Concept::Capabilities

TTX_DATA_RECORD(
    ttx_import_ops,
    TTX_DATA_MEMBER(ttx_import_ops, abstract),
    TTX_DATA_MEMBER(ttx_import_ops, visit));
TTX_DATA_RECORD(
    ttx_import,
    TTX_DATA_MEMBER(ttx_import, context),
    TTX_DATA_MEMBER(ttx_import, operations));
