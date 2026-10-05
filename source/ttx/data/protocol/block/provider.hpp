// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/form/representation.hpp"
#include "ttx/data/form/storage.hpp"
#include "ttx/data/protocol/block/provider.h"

namespace Ttx::Data::Protocol::Block {

// The admitted provider borrows each destination only through `commit`. The C++
// status reports whether the whole result can be consumed after it returns.
class Provider {
 public:
  using Api = ttx_block_provider;
  using Operations = ttx_block_provider_operations;

  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->representation &&
           api.operations->commit;
  }

  constexpr Provider(void* context, const Operations& operations)
      : api(context, &operations) {}

  constexpr explicit Provider(ttx_block_provider api) : api(api) {}

  auto get_representation() const -> const Form::Representation& {
    return *api.operations->representation(api.context);
  }

  constexpr auto get_abi() const -> ttx_block_provider { return api; }

  auto commit(Form::Storage target) const -> Status {
    return static_cast<Status>(
        api.operations->commit(api.context, target.get_abi()));
  }

 private:
  ttx_block_provider api;
};

}  // namespace Ttx::Data::Protocol::Block

TTX_DATA_RECORD(
    ttx_block_provider_operations,
    TTX_DATA_MEMBER(ttx_block_provider_operations, representation),
    TTX_DATA_MEMBER(ttx_block_provider_operations, commit));

TTX_DATA_RECORD(
    ttx_block_provider,
    TTX_DATA_MEMBER(ttx_block_provider, context),
    TTX_DATA_MEMBER(ttx_block_provider, operations));
