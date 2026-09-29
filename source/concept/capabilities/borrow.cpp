// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/capabilities/borrow.hpp"

using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

PERIMORTEM_C const ttx_representation* ttx_borrow_representation() {
  return &Binding::representation<Capabilities::Borrow>();
}

auto Capabilities::Borrow::borrow() const
    -> Perimortem::Utility::Result<Policies::Borrowed, Binding::Failure> {
  const auto api = get_abi();
  ttx_borrowed output = ttx_borrowed();
  const auto status = api.operations->borrow(api.source, &output);
  if (status == TTX_BINDING_SATISFIED) {
    if (Policies::Borrowed::accept(output)) {
      return Policies::Borrowed(output);
    }
    if (output.source && output.operations && output.operations->release) {
      output.operations->release(output.source);
    }
    return Binding::Failure::Rejected;
  }
  return status == TTX_BINDING_UNKNOWN ? Binding::Failure::Unknown
                                       : Binding::Failure::Rejected;
}
