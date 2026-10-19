// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/status.h"

namespace Ttx::Data {

// Status exposes the C Data outcomes to C++ consumers with the same numeric
// values. The supplying operation defines each outcome's effect on its output.
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
