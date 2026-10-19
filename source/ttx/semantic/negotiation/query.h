// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_NEGOTIATION_QUERY_H
#define TTX_SEMANTIC_NEGOTIATION_QUERY_H

#include "perimortem/serialization/formats/uuid.h"

#include "ttx/semantic/negotiation/binding.h"

// Query is an agreed entry point for semantic negotiation. A module can expose
// the initial Query through a described data export, while another provider can
// supply one through an already agreed contract. This initial agreement covers
// Query's record layout and callable convention so later interfaces can be
// negotiated without first constructing an Abstract graph.
//
// A UUID identifies an interface's semantic contract independently of its
// implementation, but does not describe an API record or how to call it.
// `supports` asks only whether the provider establishes that semantic promise.
// It can establish support while the API representation is still unknown.
// The provider returns Satisfied after establishing the promise,
// Unknown when the contract is unfamiliar or available evidence cannot
// determine it, and Rejected when its policy explicitly refuses the contract. A
// support answer grants no callable access and does not remove the need for
// representation agreement.
//
// `bind` asks the provider for the API identified by the same UUID. The
// consumer lends admitted destination Storage whose Representation describes
// the exact record it can consume, including callable signatures and calling
// conventions.
// The provider agrees its API with that Representation before transferring
// bytes or exposing operations from the record. A consumer needing the API can
// bind directly because a preliminary `supports` call cannot prove this
// representation agreement.
//
// The provider can select stored data, compose a record, or delegate
// production. When `bind` returns Satisfied, the destination contains the
// complete agreed API. When it returns Unknown or Rejected, the destination
// contains no usable API even if the provider touched its bytes. The consumer
// publishes the record only after Satisfied.
//
// An API record can contain an opaque context and any number of operations.
// Each supplied function interprets its own context. The provider can reuse a
// function with a compatible context or supply a wrapper that delegates to
// another implementation. A bound context can differ from this Query's context,
// and its relationship to provider storage remains private. The consumer calls
// the supplied functions and never inspects that private layout. Inspecting it
// through Query is undefined behavior.
//
// The consumer retains the complete admitted record. It obtains an embedded
// interface through that record's declared member. The member does not
// establish the layout of its enclosing record. This lets providers compose
// records while consumers depend only on their negotiated contracts.
//
// The synchronous exchange borrows the destination only until `bind` returns.
// The consumer controls those destination bytes. The supplying contract keeps
// every referenced context, description, and executable function available
// for every use permitted by the returned interface's contract. Copying a
// Query, an API record, or their pointers does not extend that access. An
// interface with independent ownership declares its retention and release
// obligations in its own contract.
//
// Both operations finish synchronously. The provider schedules no background
// work for an Unknown answer and promises no later result. A subsequent call is
// a new observation under the provider's policy. The provider can delegate an
// undetermined question as that policy permits. The consumer cannot bypass
// delegation or refusal by inspecting the opaque context.
//
// Query promises no invalidation notification or hot reload. A higher contract
// must establish those guarantees because their required behavior is specific
// to the system performing the reload.
typedef struct ttx_semantic_query {
  void* context;
  ttx_binding_status (
      *bind)(void* context, perimortem_uuid contract, ttx_storage requested);
  ttx_binding_status (*supports)(void* context, perimortem_uuid contract);
} ttx_semantic_query;

C_LINKAGE EXPORTED(TTX) const ttx_representation*
    ttx_semantic_query_representation(void);

#endif
