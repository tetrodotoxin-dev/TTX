// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_CAPABILITIES_IMPORT_H
#define TTX_GRAPH_CAPABILITIES_IMPORT_H

#include "ttx/graph/abstract.h"

#define TTX_IMPORT_ID_HIGH 0xfc74733d6cbf4e0bULL
#define TTX_IMPORT_ID_LOW 0x8353746754d8da75ULL

// Exposes the capability to interpret data described by the supplied
// Representation and supply an Abstract for the imported graph. The provider
// checks the representation before interpreting the input. On success, `visit`
// calls `callback` once with that Abstract and returns Satisfied after the
// callback finishes.
//
// The caller can supply callback state through `callback_context`, which is
// passed unchanged to `callback`. `visit` observes the input, Representation
// and callback state within one synchronous transaction and transfers no
// access to them beyond return. Input storage satisfies the extent and
// alignment of its canonical Representation. The imported graph is available
// during the callback, where the consumer can inspect it or copy the typed data
// it needs after the observation.
//
// A `visit` returning Unknown or Rejected guarantees that `callback` was never
// called. Unknown leaves the import request undetermined. Rejected explicitly
// refuses it. The provider supplies `visit` and chooses the returned graph.
typedef struct ttx_import {
  void* context;
  const struct ttx_import_ops* operations;
} ttx_import;

typedef struct ttx_import_ops {
  ttx_negotiation_status (*visit)(
      void* context,
      const void* input,
      const ttx_representation* representation,
      void* callback_context,
      void (*callback)(void* callback_context, ttx_abstract abstract));
} ttx_import_ops;

#ifdef __cplusplus

#include "perimortem/core/data.hpp"

namespace Ttx::Graph::Capabilities {

class Import {
 public:
  static constexpr auto contract_id =
      Perimortem::Core::Identity::Uuid(TTX_IMPORT_ID_HIGH, TTX_IMPORT_ID_LOW);
  using Api = ttx_import;
  using Operations = ttx_import_ops;

  // Copies the complete C record supplied under the Import contract.
  // Explicit construction preserves the provider's context and operations
  // as one API, with their supplying access duration.
  explicit constexpr Import(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

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

TTX_DATA_RECORD(ttx_import_ops, TTX_DATA_MEMBER(ttx_import_ops, visit));
TTX_DATA_RECORD(
    ttx_import,
    TTX_DATA_MEMBER(ttx_import, context),
    TTX_DATA_MEMBER(ttx_import, operations));

#endif

#endif
