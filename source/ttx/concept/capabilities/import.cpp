// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/capabilities/import.hpp"

using namespace Ttx::Concept;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

C_LINKAGE auto ttx_import_representation() -> const ttx_representation* {
  return &Form::Compiled<Form::Native<Capabilities::Import::Api>::reference>::
              get_representation()
                  .get_abi();
}
