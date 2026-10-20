// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/retained.hpp"

using namespace Ttx::Concept;
using namespace Ttx::Data;

C_LINKAGE auto ttx_retained_representation() -> const ttx_representation* {
  return &Form::Compiled<Form::Native<Policies::Retained::Api>::reference>::
              get_representation()
                  .get_abi();
}
