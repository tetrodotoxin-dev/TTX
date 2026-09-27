// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/form/representation.hpp"
#include "ttx/data/protocol/direct/provider.h"

namespace Ttx::Data::Protocol::Direct {

// The admitted C carrier can lend its public pointer immediately. Copying
// this facade keeps the same publication borrow without retaining its owner.
class Provider {
 public:
  using Api = ttx_direct_provider;
  using Operations = ttx_direct_provider_operations;

  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->representation &&
           api.operations->read_ptr;
  }

  constexpr Provider(const void* source, const Operations& operations)
      : value{
          source,
          &operations,
        } {}

  constexpr explicit Provider(ttx_direct_provider value) : value(value) {}

  auto get_representation() const -> const Form::Representation& {
    return *value.operations->representation(value.source);
  }

  constexpr auto get_abi() const -> ttx_direct_provider { return value; }

  auto read_ptr() const -> const void* {
    return value.operations->read_ptr(value.source);
  }

 private:
  ttx_direct_provider value;
};

}  // namespace Ttx::Data::Protocol::Direct

TTX_DATA_RECORD(
    ttx_direct_provider_operations,
    TTX_DATA_MEMBER(ttx_direct_provider_operations, representation),
    TTX_DATA_MEMBER(ttx_direct_provider_operations, read_ptr));

TTX_DATA_RECORD(
    ttx_direct_provider,
    TTX_DATA_MEMBER(ttx_direct_provider, source),
    TTX_DATA_MEMBER(ttx_direct_provider, operations));
