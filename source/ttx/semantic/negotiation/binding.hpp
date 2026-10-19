// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/negotiation/binding.h"

namespace Ttx::Semantic::Negotiation::Binding {

// Status describes the exchange, independently of the shape of its result.
// Typed C++ consumers return the admitted API record on success and the
// negotiation Status when that record could not be established.
enum class Status : U8 {
  Satisfied = TTX_BINDING_SATISFIED,
  Unknown = TTX_BINDING_UNKNOWN,
  Rejected = TTX_BINDING_REJECTED,
};

}  // namespace Ttx::Semantic::Negotiation::Binding
