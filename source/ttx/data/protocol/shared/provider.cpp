// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/protocol/shared/provider.hpp"

#include "ttx/data/form/compiled.hpp"

auto ttx_shared_provider_representation() -> const ttx_representation* {
  return &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
      ttx_shared_provider>::reference>::get_representation();
}
