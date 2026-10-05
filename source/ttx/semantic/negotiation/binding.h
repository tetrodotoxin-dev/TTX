// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_NEGOTIATION_BINDING_H
#define TTX_SEMANTIC_NEGOTIATION_BINDING_H

#include "ttx/data/form/storage.h"

// A UUID describes the questions an interface answers, but cannot establish
// how to call a foreign implementation. Binding therefore supplies an actual
// API record together with its canonical Representation. The consumer lends
// admitted Storage describing the record it can consume. Agreement compares
// the descriptors before any bytes are transferred or functions are called.
// Query's separate `supports` operation establishes only the semantic promise.
// Even a positive support answer leaves this representation check necessary.
//
// The record may contain an opaque context and any number of operations.
// Copying those values grants no access to the context's private layout:
// each supplied function interprets its context. A provider can reuse a
// function with a compatible context, or supply a wrapper that delegates to
// another implementation. The bound context may differ from the discovery
// context. Its relationship to provider storage remains private.
//
// Consumers retain the complete admitted record. An embedded interface is
// obtained through its declared member. It does not establish the layout of
// an enclosing record. These choices let providers compose implementations
// while the consumer depends only on the negotiated contract.
//
// This synchronous exchange borrows the destination only until return. The
// supplying provider keeps the context and executable code available
// through every use of its interfaces. An interface with independent ownership
// describes those obligations in its own contract because copying pointers
// cannot acquire a lifetime. Ordinary Flow can materialize a record when
// needed.
//
// Satisfied establishes the requested promise. Unknown supplies no determined
// answer, whether the contract is unfamiliar or the provider lacks evidence.
// Rejected is an explicit refusal of this request, not the inverse of success.
// A provider may delegate an Unknown question as permitted by its policy.
// Consumers cannot bypass that policy by inspecting its opaque context.
//
// Every answer is synchronous. Unknown schedules no work and promises no later
// answer. A subsequent observation asks again under the provider's policy.
// Neither Unknown nor Rejected supplies a usable API, even if a materializing
// provider touched the destination. Callers publish the record only on success.
//
// TODO: We need to figure out if we want to support hot reloading. Right now
// it's a higher order promise negotiated at the Concept layer or even the full
// Tetrodotoxin toolchain layer given the behavior is so domain specific but we
// should consider the invalidation case.
typedef U8 ttx_binding_status;
#define TTX_BINDING_SATISFIED ((ttx_binding_status)0)
#define TTX_BINDING_UNKNOWN ((ttx_binding_status)1)
#define TTX_BINDING_REJECTED ((ttx_binding_status)3)

// Ready records use the same byte agreement as any other data transfer.
// The supplying record and the admitted destination both have the geometry
// their descriptors promise otherwise the binding is rejected. Mismatched
// descriptors leave the destination untouched.
C_LINKAGE ttx_binding_status ttx_binding_provide(
    const ttx_representation* representation,
    const void* api,
    ttx_storage requested);

C_LINKAGE ttx_binding_status ttx_binding_marker(ttx_storage requested);

#endif
