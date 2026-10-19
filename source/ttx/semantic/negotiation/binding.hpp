// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/negotiation/binding.h"

namespace Ttx::Semantic::Negotiation::Binding {

// Status preserves the C negotiation outcomes in C++. A typed bind returns
// the complete API on success or the negotiation Status when that API could
// not be established.
enum class Status : U8 {
  Satisfied = TTX_BINDING_SATISFIED,
  Unknown = TTX_BINDING_UNKNOWN,
  Rejected = TTX_BINDING_REJECTED,
};

}  // namespace Ttx::Semantic::Negotiation::Binding
