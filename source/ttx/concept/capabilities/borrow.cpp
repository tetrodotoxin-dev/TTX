// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/capabilities/borrow.hpp"

using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

C_LINKAGE const ttx_representation* ttx_borrow_representation() {
  return &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
      Capabilities::Borrow::Api>::reference>::get_representation();
}

auto Capabilities::Borrow::borrow() const
    -> Perimortem::Core::Storage::Result<Policies::Borrowed, Binding::Status> {
  const auto api = get_abi();
  ttx_borrowed output = ttx_borrowed();
  const auto status = api.operations->borrow(api.context, &output);
  if (status == TTX_BINDING_SATISFIED) {
    if (Policies::Borrowed::accept(output)) {
      return Policies::Borrowed(output);
    }

    if (output.context && output.operations && output.operations->release) {
      output.operations->release(output.context);
    }

    return Binding::Status::Rejected;
  }

  return status == TTX_BINDING_UNKNOWN ? Binding::Status::Unknown
                                       : Binding::Status::Rejected;
}
