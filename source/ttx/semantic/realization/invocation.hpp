// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/negotiation/query.hpp"
#include "ttx/semantic/realization/invocation.h"

namespace Ttx::Semantic::Realization {

// Invocation carries the admitted call record. Its context, frame forms and
// executable code remain available through the supplying contract. Each call
// borrows input and output bytes synchronously.
class Invocation {
 public:
  using Api = ttx_invocation;

  static auto accept(Api api) -> Bool {
    return api.invoke && api.inputs && api.outputs;
  }

  explicit constexpr Invocation(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  auto accepts(
      const Data::Form::Representation& inputs,
      const Data::Form::Representation& outputs) const -> Bool {
    return inputs.compatible(*api.inputs) && outputs.compatible(*api.outputs);
  }

  auto invoke(const void* inputs, void* outputs) const -> Data::Status {
    return static_cast<Data::Status>(api.invoke(api.context, inputs, outputs));
  }

 private:
  Api api;
};

}  // namespace Ttx::Semantic::Realization

TTX_DATA_RECORD(
    ttx_invocation,
    TTX_DATA_MEMBER(ttx_invocation, context),
    TTX_DATA_MEMBER(ttx_invocation, inputs),
    TTX_DATA_MEMBER(ttx_invocation, outputs),
    TTX_DATA_MEMBER(ttx_invocation, invoke));
