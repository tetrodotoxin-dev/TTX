// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/storage/result.hpp"

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/capabilities/normalize.h"

namespace Ttx::Graph::Capabilities {

// Simulacra such as Alias can have different semantic and nominal forms. The
// semantic graph location of an abstract is globally unique (`get_abstract`
// always represents the actual Abstract), but nominal locations of intent can
// be an arbitary location and may be aliased.
//
// If an Abstract provides the Normalize capability then it provides a resolve
// operation such that:
//
//  `abstract.get_abstract() != abstract.resolve().get_abstract()`
//
// The actual resolution policy and definition of resolve is provider defined.
class Normalize {
 public:
  static constexpr auto contract_id = Perimortem::Core::Identity::Uuid(
      TTX_NORMALIZE_ID_HIGH,
      TTX_NORMALIZE_ID_LOW);
  using Api = ttx_normalize;
  using Operations = ttx_normalize_ops;

  // Copies the context and operations supplied together by a Normalize bind.
  // The provider keeps both accessible for the duration of that grant.
  explicit constexpr Normalize(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  auto resolve() const -> Perimortem::Core::Storage::
      Result<Abstract, Semantic::Negotiation::Status> {
    ttx_abstract output{};
    const auto status = Semantic::Negotiation::Status(
        api.operations->resolve(api.context, &output));
    if (status != Semantic::Negotiation::Status::Satisfied) {
      return status;
    }

    return Abstract(output);
  }

 private:
  Api api;
};

}  // namespace Ttx::Graph::Capabilities

TTX_DATA_RECORD(ttx_normalize_ops, TTX_DATA_MEMBER(ttx_normalize_ops, resolve));
TTX_DATA_RECORD(
    ttx_normalize,
    TTX_DATA_MEMBER(ttx_normalize, context),
    TTX_DATA_MEMBER(ttx_normalize, operations));
