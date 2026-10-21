// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/serialization/formats/uuid.hpp"

#include "validation/tests/graph/fixtures/value.h"

namespace Validation {

// Value calls the fixture's byte operation with its published context. The
// returned byte belongs to the caller, while the API keeps its supplying
// access duration.
class Value {
 public:
  using Api = value_api;
  static constexpr auto contract_id =
      Perimortem::Serialization::Formats::Uuid(VALUE_ID_HIGH, VALUE_ID_LOW);

  explicit constexpr Value(Api api) : api(api) {}

  auto read() const -> U8 { return api.read(api.context); }

 private:
  Api api;
};

}  // namespace Validation
