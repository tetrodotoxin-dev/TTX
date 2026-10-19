// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/tests/data/form/composition.h"

// This C operation places two copies of a canonical child form in one struct.
// Composition requests final storage after validating their placement.
ttx_data_status compose_pair(
    const ttx_representation* child,
    Count width,
    Count alignment,
    ttx_representation_allocator allocator,
    const ttx_representation** result) {
  const ttx_representation_member members[] = {
    {
      child,
      0,
    },

    {
      child,
      width,
    },
  };

  return ttx_representation_compose(
      members, 2, width * 2, alignment, allocator, result);
}
