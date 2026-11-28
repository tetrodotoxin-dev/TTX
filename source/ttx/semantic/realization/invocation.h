// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_REALIZATION_INVOCATION_H
#define TTX_SEMANTIC_REALIZATION_INVOCATION_H

#include "ttx/semantic/negotiation/query.h"

// Invocation calls an operation through represented input and output buffers.
// A consumer uses it when the payload layouts are known but the provider's
// native function declaration is private. Negotiating the operation's UUID
// and this API record establishes the call convention. The input and output
// Representations describe the buffers passed to `invoke`.
//
// The caller supplies accessible, correctly sized and aligned buffers matching
// those Representations. The provider uses them only during the synchronous
// call. The consumer uses output after TTX_DATA_SUCCESS. The operation's
// contract defines any other effects and any aliasing it permits.
//
// The supplying contract keeps context, Representations and executable code
// accessible through each call. Copying the API record preserves that access
// duration. Independent retention and release are supplied by the higher
// contract governing the operation.
typedef struct ttx_invocation {
  void* context;
  const ttx_representation* inputs;
  const ttx_representation* outputs;
  ttx_data_status (*invoke)(void* context, const void* inputs, void* outputs);
} ttx_invocation;

C_LINKAGE
EXPORTED(TTX) const ttx_representation* ttx_invocation_representation(void);

#ifdef __cplusplus

namespace Ttx::Semantic::Realization {

// Invocation uses the negotiated C call record for repeated synchronous calls.
// The consumer can compare input and output Representations with `accepts`
// before providing buffers to `invoke`. The supplying contract keeps context,
// descriptions and executable code accessible throughout those calls.
class Invocation {
 public:
  using Api = ttx_invocation;

  // Checks only that invoke, inputs and outputs are nonnull. The provider
  // must supply readable canonical descriptions, a callable function and any
  // context that function requires.
  static auto accept(Api api) -> Bool {
    return api.invoke && api.inputs && api.outputs;
  }

  // The caller establishes the record's representation and required pointers
  // before construction. This constructor only copies the record, so
  // conversion from the C record is explicit.
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
    return Data::Status(api.invoke(api.context, inputs, outputs));
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

#endif

#endif
