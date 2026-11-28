// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_ALIAS_H
#define TTX_GRAPH_POLICIES_ALIAS_H

#include "ttx/graph/abstract.h"

#define TTX_ALIAS_ID_HIGH 0xf40f9afeff4b4d64ULL
#define TTX_ALIAS_ID_LOW 0x923720295c38b2f0ULL

// Alias lets consumers share a graph reference whose target can change.
// The Abstract returned by a read is called the referent. A parser can
// publish an Alias for a declaration's type while its referent is Unknown.
// When the parser resolves the type, it redirects the Alias to the type's
// Abstract. A consumer holding the Alias can read it again to obtain the type.
//
// A successful bind of Alias supplies both operations below. The consumer
// passes the published context to each operation. Copies of this record
// access the same reference for the duration of the original grant. The
// provider decides whether to accept each request to change the referent.
//
// A successful bind of Abstract at the Alias location returns that location.
// Any other contracts or routes on the reference location belong to its
// provider. To use the referent's policies, bindings or routes, call
// get_referent and perform the observation on the returned Abstract. If a
// visitation callback redirects the Alias, the active visit still completes
// through the provider on which the consumer began it.
//
// The immediate link matters to callers inspecting the graph. If A denotes
// B and B denotes a type, reading A returns B, even after a consumer has
// obtained the type through Normalize. Redirecting B can change a later
// normalization of A. The Alias contract allows a provider to refuse a write
// or expose additional contracts at its reference location.
typedef struct ttx_alias {
  void* context;
  const struct ttx_alias_ops* operations;
} ttx_alias;

typedef struct ttx_alias_ops {
  // Every permitted read returns one complete, accessible Abstract by value,
  // including when the referent is Unknown. The consumer uses its context
  // and operation table together. If the referent is another Alias location,
  // even this same location, the read returns that Abstract directly.
  // To follow that reference, read through the Alias API of the returned
  // location as a separate observation.
  //
  // A redirection affects subsequent reads of the reference. The captured
  // Abstract and any API already bound through it remain usable for their
  // respective access grants. Observations through those records can still
  // see changes in the original provider's data.
  ttx_abstract (*get_referent)(void* context);

  // The provider returns Satisfied after publishing target as the new
  // referent. Each read then returns that Abstract until the next successful
  // replacement. A caller can set the referent to None or Unknown this way.
  // A provider that refuses the write returns Rejected. A provider that
  // cannot decide returns Unknown. In both cases the request leaves the
  // previous referent in place. The caller must supply access to the target's
  // context, table and code for the duration it agrees to publish them.
  ttx_negotiation_status (*set_referent)(void* context, ttx_abstract target);
} ttx_alias_ops;

// The built-in provider uses the address of this slot as its context. Its
// owner keeps the slot at that address while callers hold access to the slot
// or its bound Alias API. The owner synchronizes reads of the stored record
// with writes so a reader receives one provider's context and table together.
//
// The slot borrows the referent's context, operations and code. The owner
// must have access to publish that referent for as long as the slot can
// supply it. Callers that captured an earlier referent keep the access
// originally granted to them, even after the owner reuses the slot.
typedef struct ttx_alias_slot {
  ttx_abstract referent;
} ttx_alias_slot;

// Initializes writable storage with Unknown and returns the slot's Abstract.
// The following guarantees hold for every referent stored in the built-in slot.
//
// * The slot reports Reachable, including when its referent is Unknown or None.
// * The slot supports and binds Abstract and Alias. Binding Abstract returns
//   the slot. If the stored link points directly back to this slot, support
//   and bind for Normalize return Rejected. Otherwise the slot offers a
//   resolver that examines the chain when called. Other requests return
//   Rejected.
// * Every ordinary route on the slot returns None. Visiting the slot invokes
//   zero callbacks. Consumers reach the referent through the getter.
// * The setter returns Rejected if the target's context, operation table
//   or any required operation pointer is null. Otherwise it copies the record
//   and returns Satisfied. The caller supplies readable memory for a nonnull
//   table and valid code and context for every use of the published record.
//
// Each resolve follows the current chain of built-in slots. Their owners
// must coordinate writes so the traversed links stay fixed for the whole
// call, including while an independent provider resolves the last referent.
// This applies to redirects from callbacks as well as from other threads.
// Protecting each read separately could join an old link from A to B with a
// new link from B to Z after A has moved elsewhere. The slot supplies no
// locking of its own. A previously bound resolver returns Rejected while
// its origin links directly to itself. Other cycles yield Unknown.
//
// At an independent provider the resolver binds Normalize and uses the
// result of its operation. A successful foreign grant must supply a callable
// resolver and a complete result. The slot returns Rejected for an incomplete
// record or a result identical to the foreign or originating Abstract.
// For other records the provider vouches for the distinct final position.
// If the bind fails, positive support for None or Unknown establishes that
// the captured Abstract is itself final. Otherwise the resolver returns the
// failed bind's status.
// Resolving preserves every stored link and the access to earlier captures.
C_LINKAGE EXPORTED(TTX)
ttx_abstract ttx_alias_initialize(ttx_alias_slot* slot);

#endif
