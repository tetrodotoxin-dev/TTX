// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/policies/read_only.h"
#include "ttx/concept/projection.hpp"

namespace Ttx::Concept::Policies {

// ReadOnly uses Abstract's navigation under the policy declared in read_only.h.
// `provide` selects the native provider's Projection<ReadOnly> hooks so
// consumers can observe its state through that policy.
class ReadOnly : public Abstract {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_READ_ONLY_ID_HIGH, TTX_READ_ONLY_ID_LOW);
  using Api = ttx_abstract;
  using Abstract::Abstract;

  template <typename Provider>
  static auto provide(Provider& provider) -> ReadOnly {
    return ReadOnly(Projection<ReadOnly>::provide(provider).get_abi());
  }
};
}  // namespace Ttx::Concept::Policies
