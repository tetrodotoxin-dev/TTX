// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/negotiation/query.hpp"

using namespace Perimortem;
using namespace Ttx::Semantic::Negotiation;

auto Query::supports(System::Uuid contract) const -> Binding::Status {
  if (!api.supports) {
    return Binding::Status::Rejected;
  }

  const auto status = api.supports(api.context, contract);
  return status == TTX_BINDING_SATISFIED || status == TTX_BINDING_UNKNOWN
             ? static_cast<Binding::Status>(status)
             : Binding::Status::Rejected;
}

auto Query::bind(System::Uuid contract, Ttx::Data::Form::Storage requested)
    const -> Binding::Status {
  if (!api.bind) {
    return Binding::Status::Rejected;
  }

  const auto status = api.bind(api.context, contract, requested.get_abi());
  return status == TTX_BINDING_SATISFIED || status == TTX_BINDING_UNKNOWN
             ? static_cast<Binding::Status>(status)
             : Binding::Status::Rejected;
}
