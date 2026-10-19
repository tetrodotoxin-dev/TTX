// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_NEGOTIATION_BINDING_H
#define TTX_SEMANTIC_NEGOTIATION_BINDING_H

#include "ttx/data/form/storage.h"

// Negotiating operations return one of these values for each synchronous
// observation. They return Satisfied only after meeting the operation's
// contract, Unknown when the provider leaves the request undetermined, and
// Rejected when the provider explicitly refuses it. Callers use an output only
// after Satisfied. After either failure they receive no usable result and no
// promise of a later result. A subsequent call observes the provider again.
typedef U8 ttx_binding_status;
#define TTX_BINDING_SATISFIED ((ttx_binding_status)0)
#define TTX_BINDING_UNKNOWN ((ttx_binding_status)1)
#define TTX_BINDING_REJECTED ((ttx_binding_status)3)

#endif
