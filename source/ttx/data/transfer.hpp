// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/form/storage.hpp"
#include "ttx/data/transfer.h"

namespace Ttx::Data {

// The two Storage records describe the readable source and writable
// destination. Their regions remain borrowed through this synchronous copy.
class Transfer {
 public:
  static auto copy(Form::Storage input, Form::Storage output) -> Status {
    const auto source = input.get_abi();
    const auto target = output.get_abi();
    return static_cast<Status>(ttx_transfer(&source, &target));
  }
};

}  // namespace Ttx::Data
