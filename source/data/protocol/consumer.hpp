// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/form/representation.hpp"
#include "ttx/data/protocol/consumer.h"

namespace Ttx::Data::Protocol {

// This value retains the admitted C carrier while borrowing its requirement.
// Copies ask the same consumer policy without acquiring another lifetime.
class Consumer {
 public:
  using Api = ttx_consumer;
  using Operations = ttx_consumer_operations;

  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->representation;
  }

  constexpr Consumer(const void* source, const Operations& operations)
      : value(source, &operations) {}

  constexpr explicit Consumer(ttx_consumer value) : value(value) {}

  auto get_representation() const -> const Form::Representation& {
    return *value.operations->representation(value.source);
  }

  constexpr auto get_abi() const -> ttx_consumer { return value; }

 private:
  ttx_consumer value;
};

}  // namespace Ttx::Data::Protocol

TTX_DATA_RECORD(
    ttx_consumer_operations,
    TTX_DATA_MEMBER(ttx_consumer_operations, representation));

TTX_DATA_RECORD(
    ttx_consumer,
    TTX_DATA_MEMBER(ttx_consumer, source),
    TTX_DATA_MEMBER(ttx_consumer, operations));
