// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/negotiation/query.h"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

auto Negotiation::Query::supports(Identity::Uuid contract) const
    -> Negotiation::Status {
  if (!api.supports) {
    return Negotiation::Status::Rejected;
  }

  const auto status = api.supports(api.context, contract);
  return status == Negotiation::Status::Satisfied ||
                 status == Negotiation::Status::Unknown
             ? status
             : Negotiation::Status::Rejected;
}

auto Negotiation::Query::bind(Identity::Uuid contract, Form::Storage requested)
    const -> Negotiation::Status {
  if (!api.bind || ttx_storage_check(requested.get_abi()) != TTX_DATA_SUCCESS) {
    return Negotiation::Status::Rejected;
  }

  const auto status = api.bind(api.context, contract, requested.get_abi());
  return status == Negotiation::Status::Satisfied ||
                 status == Negotiation::Status::Unknown
             ? status
             : Negotiation::Status::Rejected;
}

auto ttx_semantic_query_representation() -> const ttx_representation* {
  return &Form::Compiled<
              Form::Native<ttx_semantic_query>::reference>::get_representation()
              .get_abi();
}
