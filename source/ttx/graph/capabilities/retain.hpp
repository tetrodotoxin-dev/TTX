// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/capabilities/retain.h"
#include "ttx/graph/policies/retained.hpp"

namespace Ttx::Graph::Capabilities {

// Retain asks the provider to supply an Abstract that remains usable after
// the current observation ends. The returned Retained interface carries the
// consumer's release obligation. The provider can keep the original state
// available or supply different state that preserves the Abstract's policies.
//
// `retain` returns a Result containing the Retained interface or a status.
// Unknown means the provider leaves its ability to supply that interface
// undetermined. Rejected means the provider refuses to supply it. Each
// successful call requires the consumer to call `release` on its Retained
// result once, even when several results use the same context.
//
// Before returning Satisfied, the provider supplies a nonnull context and every
// Retained operation, including the Abstract functions. The C++ `retain` method
// checks those pointers before constructing the result. If any required pointer
// is missing, the method returns Rejected. When the incomplete record still has
// a context and a `release` function, the method calls that function with the
// returned context first, fulfilling the result's release obligation.
class Retain {
 public:
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_RETAIN_ID_HIGH,
      TTX_RETAIN_ID_LOW);
  using Api = ttx_retain;
  using Operations = ttx_retain_ops;

  // Returns true when the API record's context, table, `retain` function and
  // every Abstract function pointer are nonnull. Any present table must be
  // readable and match the negotiated representation.
  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->retain &&
           Abstract::accept(
               ttx_abstract(api.context, &api.operations->abstract));
  }

  // Copies a ttx_retain API record with the agreed representation and nonnull
  // required pointers. The caller checks those conditions before construction.
  // Explicit construction marks this unchecked conversion from the C record.
  explicit constexpr Retain(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract {
    return Abstract(api.context, api.operations->abstract);
  }

  EXPORTED(TTX) auto retain() const -> Perimortem::Core::Storage::
      Result<Policies::Retained, Semantic::Negotiation::Status>;

 private:
  Api api;
};

}  // namespace Ttx::Graph::Capabilities

TTX_DATA_RECORD(
    ttx_retain_ops,
    TTX_DATA_MEMBER(ttx_retain_ops, abstract),
    TTX_DATA_MEMBER(ttx_retain_ops, retain));
TTX_DATA_RECORD(
    ttx_retain,
    TTX_DATA_MEMBER(ttx_retain, context),
    TTX_DATA_MEMBER(ttx_retain, operations));
