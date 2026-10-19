// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/negotiation/query.hpp"

using namespace Perimortem;
using namespace Ttx::Semantic::Negotiation;

auto Query::supports(Serialization::Formats::Uuid contract) const -> Binding::Status {
  if (!api.supports) {
    return Binding::Status::Rejected;
  }

  const auto status = api.supports(api.context, contract);
  return status == TTX_BINDING_SATISFIED || status == TTX_BINDING_UNKNOWN
             ? static_cast<Binding::Status>(status)
             : Binding::Status::Rejected;
}

auto Query::bind(Serialization::Formats::Uuid contract, Ttx::Data::Form::Storage requested)
    const -> Binding::Status {
  if (!api.bind || ttx_storage_check(requested.get_abi()) != TTX_DATA_SUCCESS) {
    return Binding::Status::Rejected;
  }

  const auto status = api.bind(api.context, contract, requested.get_abi());
  return status == TTX_BINDING_SATISFIED || status == TTX_BINDING_UNKNOWN
             ? static_cast<Binding::Status>(status)
             : Binding::Status::Rejected;
}

auto ttx_semantic_query_representation() -> const ttx_representation* {
  return &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
      ttx_semantic_query>::reference>::get_representation();
}
