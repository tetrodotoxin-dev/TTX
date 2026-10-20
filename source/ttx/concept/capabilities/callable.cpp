// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/capabilities/callable.hpp"

using namespace Ttx::Data;

auto ttx_callable_representation() -> const ttx_representation* {
  return &Form::Compiled<
              Form::Native<ttx_callable>::reference>::get_representation()
              .get_abi();
}
