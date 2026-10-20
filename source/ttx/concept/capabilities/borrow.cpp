// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/capabilities/borrow.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Concept;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

C_LINKAGE auto ttx_borrow_representation() -> const ttx_representation* {
  return &Form::Compiled<
      Form::Native<Capabilities::Borrow::Api>::reference>::get_representation();
}

auto Capabilities::Borrow::borrow() const
    -> Storage::Result<Policies::Borrowed, Negotiation::Status> {
  const auto api = get_abi();
  ttx_borrowed output = ttx_borrowed();
  const auto status = api.operations->borrow(api.context, &output);
  if (status == TTX_NEGOTIATION_SATISFIED) {
    if (Policies::Borrowed::accept(output)) {
      return Policies::Borrowed(output);
    }

    if (output.context && output.operations && output.operations->release) {
      output.operations->release(output.context);
    }

    return Negotiation::Status::Rejected;
  }

  return status == TTX_NEGOTIATION_UNKNOWN ? Negotiation::Status::Unknown
                                           : Negotiation::Status::Rejected;
}
