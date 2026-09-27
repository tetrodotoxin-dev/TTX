// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/protocol/consumer.hpp"

#include "ttx/data/form/compiled.hpp"

auto ttx_consumer_representation() -> const ttx_representation* {
  return &Ttx::Data::Form::Compiled<
      Ttx::Data::Form::Native<ttx_consumer>::reference>::get_representation();
}
