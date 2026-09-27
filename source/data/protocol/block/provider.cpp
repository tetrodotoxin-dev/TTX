// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/protocol/block/provider.hpp"

#include "ttx/data/form/compiled.hpp"

auto ttx_block_provider_representation() -> const ttx_representation* {
  return &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
      ttx_block_provider>::reference>::get_representation();
}
