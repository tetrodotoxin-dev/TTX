// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/capabilities/retain.h"
#include "ttx/graph/policies/retained.hpp"

namespace Ttx::Graph::Capabilities {

// Retain asks the provider to supply an Abstract that remains usable after
// its current access duration ends. The returned Retained interface carries
// the consumer's release obligation. The provider can keep the original state
// available or supply different state that preserves the Abstract's policies.
//
// `retain` returns a Result containing the Retained interface or a status.
// Unknown means the provider leaves its ability to supply that interface
// undetermined. Rejected means the provider refuses to supply it. Each
// successful call requires the consumer to call `release` on its Retained
// result once, even when several results use the same context.
//
// A successful call supplies a complete Retained contract. Its provider
// keeps the returned Abstract available until the matching release.
class Retain {
 public:
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_RETAIN_ID_HIGH,
      TTX_RETAIN_ID_LOW);
  using Api = ttx_retain;
  using Operations = ttx_retain_ops;

  // Copies a complete C record supplied under the Retain contract. Explicit
  // construction keeps its context and operations paired under their supplying
  // access contract.
  explicit constexpr Retain(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  // Returns to the graph location that supplied this binding. The provider
  // supplies the complete Abstract and may use a different context and table.
  auto get_abstract() const -> Abstract {
    return Abstract(api.operations->get_abstract(api.context));
  }

  EXPORTED(TTX) auto retain() const -> Perimortem::Core::Storage::
      Result<Policies::Retained, Semantic::Negotiation::Status>;

 private:
  Api api;
};

}  // namespace Ttx::Graph::Capabilities
