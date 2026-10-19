// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/abstract.hpp"

auto ttx_abstract_representation() -> const ttx_representation* {
  return &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
      Ttx::Concept::Abstract::Api>::reference>::get_representation();
}
