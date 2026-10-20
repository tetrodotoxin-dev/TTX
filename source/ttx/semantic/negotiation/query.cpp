// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/negotiation/query.hpp"

using namespace Perimortem::Serialization;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

auto Negotiation::Query::supports(Formats::Uuid contract) const
    -> Negotiation::Status {
  if (!api.supports) {
    return Negotiation::Status::Rejected;
  }

  const auto status = api.supports(api.context, contract);
  return status == TTX_NEGOTIATION_SATISFIED ||
                 status == TTX_NEGOTIATION_UNKNOWN
             ? Negotiation::Status(status)
             : Negotiation::Status::Rejected;
}

auto Negotiation::Query::bind(Formats::Uuid contract, Form::Storage requested)
    const -> Negotiation::Status {
  if (!api.bind || ttx_storage_check(requested.get_abi()) != TTX_DATA_SUCCESS) {
    return Negotiation::Status::Rejected;
  }

  const auto status = api.bind(api.context, contract, requested.get_abi());
  return status == TTX_NEGOTIATION_SATISFIED ||
                 status == TTX_NEGOTIATION_UNKNOWN
             ? Negotiation::Status(status)
             : Negotiation::Status::Rejected;
}

auto ttx_semantic_query_representation() -> const ttx_representation* {
  return &Form::Compiled<
              Form::Native<ttx_semantic_query>::reference>::get_representation()
              .get_abi();
}
