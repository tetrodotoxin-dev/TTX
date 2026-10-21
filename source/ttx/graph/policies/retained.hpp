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

  // Copies a ttx_retained API record under the original result's release
  // obligation. The provider supplies its complete operations. Explicit
  // construction keeps that record paired with its original context.
  explicit constexpr Retained(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  // Supplies the retained Abstract with its own context and operations. A
  // record obtained by binding returns to that binding's graph location.
  auto get_abstract() const -> Abstract {
    return Abstract(api.operations->get_abstract(api.context));
  }

  auto release() const -> void {
    const auto api = get_abi();
    api.operations->release(api.context);
  }

 private:
  Api api;
};

}  // namespace Ttx::Graph::Policies
