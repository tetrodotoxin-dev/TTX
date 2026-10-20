// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/capabilities/retain.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Concept;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

C_LINKAGE auto ttx_retain_representation() -> const ttx_representation* {
  return &Form::Compiled<Form::Native<Capabilities::Retain::Api>::reference>::
              get_representation()
                  .get_abi();
}

auto Capabilities::Retain::retain() const
    -> Storage::Result<Policies::Retained, Negotiation::Status> {
  const auto api = get_abi();
  ttx_retained output = ttx_retained();
  const auto status = api.operations->retain(api.context, &output);
  if (status == TTX_NEGOTIATION_SATISFIED) {
    if (Policies::Retained::accept(output)) {
      return Policies::Retained(output);
    }

    if (output.context && output.operations && output.operations->release) {
      output.operations->release(output.context);
    }

    return Negotiation::Status::Rejected;
  }

  return status == TTX_NEGOTIATION_UNKNOWN ? Negotiation::Status::Unknown
                                           : Negotiation::Status::Rejected;
}
