// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/form/storage.hpp"
#include "ttx/data/transfer.h"

namespace Ttx::Data {

// Transfer copies a represented value through the C Data operation. The two
// Storage records supply source and destination geometry. Their referenced
// memory stays accessible until the synchronous copy returns. The operation
// copies only the represented extent after checking both regions. A failed
// check leaves destination bytes unchanged.
class Transfer {
 public:
  static auto copy(Form::Storage input, Form::Storage output) -> Status {
    const auto source = input.get_abi();
    const auto target = output.get_abi();
    return Status(ttx_transfer(&source, &target));
  }
};

}  // namespace Ttx::Data
