// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/form/representation.hpp"
#include "ttx/data/protocol/consumer.h"

namespace Ttx::Data::Protocol {

// This C++ interface stores the API record and borrows its representation.
// Copies ask the same consumer policy without acquiring another lifetime.
class Consumer {
 public:
  using Api = ttx_consumer;
  using Operations = ttx_consumer_operations;

  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->representation;
  }

  constexpr Consumer(void* context, const Operations& operations)
      : api(context, &operations) {}

  constexpr explicit Consumer(ttx_consumer api) : api(api) {}

  auto get_representation() const -> const Form::Representation& {
    return *api.operations->representation(api.context);
  }

  constexpr auto get_abi() const -> ttx_consumer { return api; }

 private:
  ttx_consumer api;
};

}  // namespace Ttx::Data::Protocol

TTX_DATA_RECORD(
    ttx_consumer_operations,
    TTX_DATA_MEMBER(ttx_consumer_operations, representation));

TTX_DATA_RECORD(
    ttx_consumer,
    TTX_DATA_MEMBER(ttx_consumer, context),
    TTX_DATA_MEMBER(ttx_consumer, operations));
