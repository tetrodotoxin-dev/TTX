// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_NEGOTIATION_QUERY_H
#define TTX_SEMANTIC_NEGOTIATION_QUERY_H

#include "perimortem/serialization/formats/uuid.h"

#include "ttx/semantic/negotiation/binding.h"

// Query negotiates semantic contracts before a consumer enters an Abstract
// graph. The requested UUID selects meaning, while destination Storage
// describes the exact API record the consumer can use, including function
// signatures and calling conventions. A module can publish the initial Query
// as described data, and another negotiated interface can supply later Queries.
// The supplying contract establishes this Query's own record representation and
// calling convention before either operation is called. A module bootstrap
// does that by transferring its described export into the consumer's known
// Query representation.
//
// `supports` reports Satisfied when the provider establishes the UUID's
// promise, Unknown when it leaves that promise undetermined, and Rejected when
// it explicitly refuses it. This checks meaning independently of
// representation. A consumer obtains callable operations through `bind`, which
// can be called directly with the required UUID and Storage.
//
// `bind` agrees the provider's API representation with the destination before
// transferring the complete record. The consumer uses that record only after
// Satisfied. Unknown and Rejected leave the destination unusable even if the
// provider changed its bytes. Both operations finish synchronously. A later
// call makes a new observation under the provider's policy. Each status
// completes that call. Obtaining another result requires another request.
//
// The provider can construct the record, reuse stored functions with a suitable
// context, or delegate production. Each supplied function interprets its own
// context. The returned context can differ from the Query's context. Consumers
// call the supplied functions with their supplied context. Inspecting its
// private layout through Query is undefined behavior.
//
// The consumer keeps the complete negotiated record. Its declared members
// provide access to any embedded interfaces. Knowledge of an embedded interface
// establishes that member's layout, while using its enclosing record requires
// agreement on the enclosing representation.
//
// The destination is borrowed until `bind` returns. The supplying contracts
// keep referenced state, descriptions and executable code accessible through
// every permitted use. Copying an API record preserves that access duration.
// Retention, release, invalidation and reload belong to the interfaces that
// explicitly provide those guarantees.
typedef struct ttx_semantic_query {
  void* context;
  ttx_binding_status (
      *bind)(void* context, perimortem_uuid contract, ttx_storage requested);
  ttx_binding_status (*supports)(void* context, perimortem_uuid contract);
} ttx_semantic_query;

C_LINKAGE EXPORTED(TTX) const ttx_representation*
    ttx_semantic_query_representation(void);

#endif
