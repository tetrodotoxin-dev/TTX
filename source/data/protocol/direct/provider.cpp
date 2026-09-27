// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/protocol/direct/provider.hpp"

#include "ttx/data/form/compiled.hpp"

auto ttx_direct_provider_representation() -> const ttx_representation* {
  return &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
      ttx_direct_provider>::reference>::get_representation();
}
