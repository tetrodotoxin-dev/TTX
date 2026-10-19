// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/negotiation/query.hpp"
#include "ttx/semantic/realization/invocation.h"

namespace Ttx::Semantic::Realization {

// Invocation uses the negotiated C call record for repeated synchronous calls.
// The consumer can compare input and output Representations with `accepts`
// before providing buffers to `invoke`. The supplying contract keeps context,
// descriptions and executable code accessible throughout those calls.
class Invocation {
 public:
  using Api = ttx_invocation;

  // Checks that the call and both description pointers are present. The
  // supplying contract establishes readable canonical descriptions, callable
  // code and any context required by that code.
  static auto accept(Api api) -> Bool {
    return api.invoke && api.inputs && api.outputs;
  }

  // Copies a record whose representation and required pointers the caller has
  // established. Construction performs no validation. Explicit construction
  // marks that unchecked conversion from the C record.
  explicit constexpr Invocation(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  // Compares the complete canonical input and output forms. The caller keeps
  // both supplied descriptions readable. Buffer capacity and alignment remain
  // caller obligations because this operation receives descriptions only.
  auto accepts(
      const Data::Form::Representation& inputs,
      const Data::Form::Representation& outputs) const -> Bool {
    return inputs.compatible(*api.inputs) && outputs.compatible(*api.outputs);
  }

  // Calls the supplied function directly. The caller provides buffers matching
  // the negotiated descriptions and keeps them accessible through the call.
  // The operation returns the provider's status without repeating layout or
  // buffer checks. Output becomes usable after Success under that contract.
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
