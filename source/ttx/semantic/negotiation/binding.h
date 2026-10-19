// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_NEGOTIATION_BINDING_H
#define TTX_SEMANTIC_NEGOTIATION_BINDING_H

#include "ttx/data/form/storage.h"

// Negotiation reports the provider's decision for one synchronous operation.
// Satisfied means the provider fulfilled the requested contract. Unknown leaves
// the request undetermined. Rejected explicitly refuses it. The consumer uses
// output only after Satisfied. Each subsequent call observes the provider again
// and can produce a different status under the same contract.
typedef U8 ttx_binding_status;

#define TTX_BINDING_SATISFIED ((ttx_binding_status)0)
#define TTX_BINDING_UNKNOWN ((ttx_binding_status)1)
#define TTX_BINDING_REJECTED ((ttx_binding_status)3)

#endif
