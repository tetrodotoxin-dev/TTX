// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/negotiation/query.hpp"
#include "ttx/semantic/realization/invocation.h"

namespace Ttx::Semantic::Realization {

// Invocation retains the agreed operation record so repeated calls can use its
// function directly without another UUID lookup, signature walk or table
// transfer. The provider keeps the context and code available through every
// invocation.
class Invocation {
 public:
  auto connect(
      Negotiation::Query instance,
      Perimortem::System::Uuid contract,
      const Data::Form::Representation& inputs,
      const Data::Form::Representation& outputs)
      -> Negotiation::Binding::Status;

  auto invoke(const void* inputs, void* outputs) const -> Data::Status {
    return static_cast<Data::Status>(
        call.invoke(call.context, inputs, outputs));
  }

  auto close() -> void { call = ttx_invocation(); }

 private:
  ttx_invocation call = ttx_invocation();
};

}  // namespace Ttx::Semantic::Realization

TTX_DATA_RECORD(
    ttx_invocation,
    TTX_DATA_MEMBER(ttx_invocation, context),
    TTX_DATA_MEMBER(ttx_invocation, inputs),
    TTX_DATA_MEMBER(ttx_invocation, outputs),
    TTX_DATA_MEMBER(ttx_invocation, invoke));
