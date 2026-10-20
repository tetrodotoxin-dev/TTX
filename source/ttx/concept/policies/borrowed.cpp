// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/borrowed.hpp"

using namespace Ttx::Concept;
using namespace Ttx::Data;

C_LINKAGE auto ttx_borrowed_representation() -> const ttx_representation* {
  return &Form::Compiled<
      Form::Native<Policies::Borrowed::Api>::reference>::get_representation();
}
