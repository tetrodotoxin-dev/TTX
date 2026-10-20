// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/negotiation/status.h"

namespace Ttx::Semantic::Negotiation {

// Status reports whether a provider satisfied, left undetermined or rejected
// a negotiation request. Its values match ttx_negotiation_status, so the result
// can cross C and C++ interfaces unchanged. A typed bind returns the complete
// API on Satisfied. Otherwise it returns the provider's status or Rejected
// when the returned record fails the interface's checks.
enum class Status : U8 {
  Satisfied = TTX_NEGOTIATION_SATISFIED,
  Unknown = TTX_NEGOTIATION_UNKNOWN,
  Rejected = TTX_NEGOTIATION_REJECTED,
};

}  // namespace Ttx::Semantic::Negotiation
