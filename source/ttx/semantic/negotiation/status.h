// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_NEGOTIATION_STATUS_H
#define TTX_SEMANTIC_NEGOTIATION_STATUS_H

#include "toolchain/toolchain.h"

// Negotiation reports the provider's decision for one synchronous operation.
// Satisfied means the provider fulfilled the requested contract. Unknown leaves
// the request undetermined. Rejected explicitly refuses it. The consumer uses
// output only after Satisfied. Each subsequent call observes the provider again
// and can produce a different status under the same contract.
//
// A C++ provider can return Negotiation::Status directly from a callback in a
// C operation table. C reads the same result as an unsigned byte.
#define TTX_NEGOTIATION_SATISFIED_CODE 0
#define TTX_NEGOTIATION_UNKNOWN_CODE 1
#define TTX_NEGOTIATION_REJECTED_CODE 3

#ifdef __cplusplus

namespace Ttx::Semantic::Negotiation {

enum class Status : U8 {
  Satisfied = TTX_NEGOTIATION_SATISFIED_CODE,
  Unknown = TTX_NEGOTIATION_UNKNOWN_CODE,
  Rejected = TTX_NEGOTIATION_REJECTED_CODE,
};

}  // namespace Ttx::Semantic::Negotiation

using ttx_negotiation_status = Ttx::Semantic::Negotiation::Status;

#else

typedef U8 ttx_negotiation_status;

#endif

#define TTX_NEGOTIATION_SATISFIED \
  ((ttx_negotiation_status)TTX_NEGOTIATION_SATISFIED_CODE)
#define TTX_NEGOTIATION_UNKNOWN \
  ((ttx_negotiation_status)TTX_NEGOTIATION_UNKNOWN_CODE)
#define TTX_NEGOTIATION_REJECTED \
  ((ttx_negotiation_status)TTX_NEGOTIATION_REJECTED_CODE)

#endif
