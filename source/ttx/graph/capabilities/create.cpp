// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/capabilities/create.hpp"

using namespace Ttx::Graph;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

C_LINKAGE auto ttx_create_representation() -> const ttx_representation* {
  return &Form::Compiled<Form::Native<Capabilities::Create::Api>::reference>::
              get_representation()
                  .get_abi();
}
