// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/form/representation.hpp"
#include "ttx/data/protocol/shared/lifetime.hpp"
#include "ttx/data/protocol/shared/provider.h"

namespace Ttx::Data::Protocol::Shared {

// A successful C acquisition becomes an owning Lifetime here. Its move
// transfers the release obligation, while this provider remains borrowed.
class Provider {
 public:
  using Api = ttx_shared_provider;
  using Operations = ttx_shared_provider_operations;

  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->representation &&
           api.operations->acquire;
  }

  constexpr Provider(void* source, const Operations& operations)
      : value(source, &operations) {}

  constexpr explicit Provider(ttx_shared_provider value) : value(value) {}

  auto get_representation() const -> const Form::Representation& {
    return *value.operations->representation(value.source);
  }

  constexpr auto get_abi() const -> ttx_shared_provider { return value; }

  auto acquire() const -> Perimortem::Utility::Result<Lifetime, Status> {
    ttx_shared_lifetime result;
    const auto status = value.operations->acquire(value.source, &result);
    if (status != TTX_DATA_SUCCESS) {
      return static_cast<Status>(status);
    }

    return Lifetime(result);
  }

 private:
  ttx_shared_provider value;
};

}  // namespace Ttx::Data::Protocol::Shared

TTX_DATA_RECORD(
    ttx_shared_provider_operations,
    TTX_DATA_MEMBER(ttx_shared_provider_operations, representation),
    TTX_DATA_MEMBER(ttx_shared_provider_operations, acquire));

TTX_DATA_RECORD(
    ttx_shared_provider,
    TTX_DATA_MEMBER(ttx_shared_provider, source),
    TTX_DATA_MEMBER(ttx_shared_provider, operations));
