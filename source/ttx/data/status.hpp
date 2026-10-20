// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/status.h"

namespace Ttx::Data {

// Status is the C++ form of ttx_data_status. Each enumerator keeps its C value
// so the same result can pass across either interface. What a failure means
// for the output depends on the operation. For example, Transfer leaves the
// destination unchanged when it rejects a copy.
enum class Status : U8 {
  Success = TTX_DATA_SUCCESS,
  Invalid = TTX_DATA_INVALID,
  Bounds = TTX_DATA_BOUNDS,
  Overflow = TTX_DATA_OVERFLOW,
  Incompatible = TTX_DATA_INCOMPATIBLE,
  Unsupported = TTX_DATA_UNSUPPORTED,
  Denied = TTX_DATA_DENIED,
};

}  // namespace Ttx::Data
