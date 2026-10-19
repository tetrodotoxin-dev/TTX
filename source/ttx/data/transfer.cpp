// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/transfer.h"

#include <string.h>

#include "ttx/data/form/representation.hpp"

auto ttx_transfer(const ttx_storage* input, const ttx_storage* output)
    -> ttx_data_status {
  if (!input || !output || !input->representation || !output->representation ||
      !input->representation->data || !output->representation->data) {
    return TTX_DATA_INVALID;
  }

  // Compare the readable descriptor bytes with the admitted destination form
  // before decoding source geometry. A bootstrap can therefore reject a
  // foreign form without following its encoded references.
  if (!input->representation->compatible(*output->representation)) {
    return TTX_DATA_INCOMPATIBLE;
  }

  const auto source = ttx_storage_check(*input);
  if (source != TTX_DATA_SUCCESS) {
    return source;
  }

  const auto target = ttx_storage_check(*output);
  if (target != TTX_DATA_SUCCESS) {
    return target;
  }

  const Count size = output->representation->get_extent();
  if (size) {
    memmove(output->data, input->data, size);
  }

  return TTX_DATA_SUCCESS;
}
