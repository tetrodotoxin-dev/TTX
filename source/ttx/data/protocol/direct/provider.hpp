// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/form/representation.hpp"
#include "ttx/data/protocol/direct/provider.h"

namespace Ttx::Data::Protocol::Direct {

// The API record supplies the operation that lends the payload pointer.
// Copies borrow the same context and operations for the supplying lifetime.
class Provider {
 public:
  using Api = ttx_direct_provider;
  using Operations = ttx_direct_provider_operations;

  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->representation &&
           api.operations->read_ptr;
  }

  constexpr Provider(void* context, const Operations& operations)
      : api(context, &operations) {}

  constexpr explicit Provider(ttx_direct_provider api) : api(api) {}

  auto get_representation() const -> const Form::Representation& {
    return *api.operations->representation(api.context);
  }

  constexpr auto get_abi() const -> ttx_direct_provider { return api; }

  auto read_ptr() const -> const void* {
    return api.operations->read_ptr(api.context);
  }

 private:
  ttx_direct_provider api;
};

}  // namespace Ttx::Data::Protocol::Direct

TTX_DATA_RECORD(
    ttx_direct_provider_operations,
    TTX_DATA_MEMBER(ttx_direct_provider_operations, representation),
    TTX_DATA_MEMBER(ttx_direct_provider_operations, read_ptr));

TTX_DATA_RECORD(
    ttx_direct_provider,
    TTX_DATA_MEMBER(ttx_direct_provider, context),
    TTX_DATA_MEMBER(ttx_direct_provider, operations));
