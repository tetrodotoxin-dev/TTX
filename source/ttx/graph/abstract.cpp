// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/abstract.hpp"

using namespace Ttx::Graph;
using namespace Ttx::Data;

auto ttx_abstract_representation() -> const ttx_representation* {
  return &Form::Compiled<
              Form::Native<Abstract::Api>::reference>::get_representation()
              .get_abi();
}
