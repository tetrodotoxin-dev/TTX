// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/retained.h"

namespace Ttx::Graph::Policies {

// Retained supplies an Abstract with an explicit `release` operation. The
// provider keeps the Abstract's context, operation table and referenced
// metadata available until the consumer calls `release`.
//
// Copies of the API record and interfaces bound through its Abstract share
// the result's single release obligation. The consumer calls `release` after
// finishing all uses of those records and interfaces. Using any of them after
// that release is undefined behavior. Calling `retain` again creates a separate
// result with its own release obligation.
class Retained {
 public:
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_RETAINED_ID_HIGH,
      TTX_RETAINED_ID_LOW);
  using Api = ttx_retained;
  using Operations = ttx_retained_ops;

  // Returns true when the API record's context, table, `release` function and
  // every Abstract function pointer are nonnull. Any present table must be
  // readable and match the negotiated representation.
  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->release &&
           Abstract::accept(
               ttx_abstract(api.context, &api.operations->abstract));
  }

  // Copies a ttx_retained API record under the original result's release
  // obligation. The caller checks the record's representation and required
  // pointers before construction. Explicit construction marks this unchecked
  // conversion from the C record.
  explicit constexpr Retained(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract {
    return Abstract(api.context, api.operations->abstract);
  }

  auto release() const -> void {
    const auto api = get_abi();
    api.operations->release(api.context);
  }

 private:
  Api api;
};

}  // namespace Ttx::Graph::Policies

TTX_DATA_RECORD(
    ttx_retained_ops,
    TTX_DATA_MEMBER(ttx_retained_ops, abstract),
    TTX_DATA_MEMBER(ttx_retained_ops, release));
TTX_DATA_RECORD(
    ttx_retained,
    TTX_DATA_MEMBER(ttx_retained, context),
    TTX_DATA_MEMBER(ttx_retained, operations));
