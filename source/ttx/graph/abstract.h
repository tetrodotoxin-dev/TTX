// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_ABSTRACT_H
#define TTX_GRAPH_ABSTRACT_H

#include "toolchain/toolchain.h"

#include "perimortem/core/identity/uuid.h"
#include "perimortem/core/view/bytes.h"

#include "ttx/data/form/representation.h"
#include "ttx/semantic/negotiation/status.h"

// The Abstract UUID identifies its complete callable ABI. Changing the
// published operations requires a new UUID for bootstrap negotiation.
#define TTX_ABSTRACT_ID_HIGH 0xb427e8d104cf46b1ULL
#define TTX_ABSTRACT_ID_LOW 0x83a7da1ee62a518bULL

// None and Unknown use reserved UUIDs in Graph. These identifiers describe the
// axiomatic graph results. Other UUIDs identify independently chosen contracts.
#define TTX_NONE_ID_HIGH 0ULL
#define TTX_NONE_ID_LOW 0ULL
#define TTX_UNKNOWN_ID_HIGH 0ULL
#define TTX_UNKNOWN_ID_LOW 1ULL

// State classifies a graph result as None, Unknown or Present. Present
// identifies a graph node even when the consumer recognizes none of its
// contracts. A Present node must reject both reserved UUIDs, None and Unknown,
// through `supports` and `bind`. Requests for other contracts can return
// Satisfied, Unknown or Rejected.
//
// `get_state` supplies this classification in one call. The reserved UUIDs
// let None and Unknown participate in contract binding.
typedef U8 ttx_abstract_state;

#define TTX_ABSTRACT_NONE ((ttx_abstract_state)0)
#define TTX_ABSTRACT_UNKNOWN ((ttx_abstract_state)1)
#define TTX_ABSTRACT_PRESENT ((ttx_abstract_state)2)

// Abstracts connect through route operations to form a graph independent of
// any host language's native type system. This lets providers and consumers
// navigate the same semantic relationships across languages and runtimes,
// then bind UUID contracts to obtain concrete APIs in their native type
// systems. TTX treats operations on those typed values as terminals that
// leave graph navigation and follow the bound contract.
//
// Semantic negotiates the Abstract ABI using Data's representations and
// transfer operations. Through that interface, observations stay within the
// graph: `resolve_route` and `visit_routes` supply further Abstracts.
//
//   Abstract --observation--> Abstract --bind(UUID)--> Type
//   Type     --terminal-----> Type
//   Type     --operation----> Abstract (any graph location)
//
// A successful bind supplies the complete requested contract, including every
// required operation and policy. The guarantee applies to that returned API.
// Each further binding is a separate request whose success depends on the
// provider supplying that API. For UUIDs other than the reserved None and
// Unknown, the provider can refuse further requests, including the bound
// contract's own UUID or Abstract, while its supplied operations remain usable.
// Requests for the reserved UUIDs must follow `get_state`'s rules.
//
// A typed operation can return an Abstract anywhere in the graph. The optional
// `get_abstract` contract provides a way back: after a successful bind, it
// returns an Abstract at the same graph location that supplied the binding.
// Repeating bind/get_abstract round trips therefore preserves that location,
// even if each binding publishes a different API record. Each bind can still
// fail independently.
//
//   A --bind(Type)--> Type --get_abstract()--> A'
//
// A and A' represent the same graph location. This is semantic equivalence,
// with each returned API keeping its own published context and operations.
// Constant makes this relationship important: its promise fixes the location
// from which it was bound. Returning to that location keeps the promise
// attached to the graph position it qualifies, while data and outgoing
// observations remain governed by their own contracts. Lifetime is separate.
//
// An ordinary node remains in the graph even when the consumer recognizes
// none of its contract UUIDs or routes.
//
// None and Unknown are the graph's axiomatic results. None marks an explicit
// dead end, while Unknown leaves the route's result undetermined. Each returns
// itself for every further route, preserving the outcome independently of
// which step reached it. Retrying means asking the original Abstract again.
//
//   None    --any route--> None
//   Unknown --any route--> Unknown
//
// The supplying contract establishes the complete API representation before
// the consumer calls its operations. That representation includes the table's
// C calling conventions and recursive Abstract results. A usable Abstract has
// a nonnull context, a readable operation table and every operation populated.
// Each API is valid only as its published construction of context and
// operations. The supplied functions interpret that context's private layout.
// A provider can publish several interfaces using the same context with
// different operations. Consumers preserve the pairing supplied for each API.
//
// The supplying contract defines how long the context and operation table
// remain accessible. Copies of the Abstract share that duration. Retained
// access requires a contract that provides it.
// Later observations can select different Abstracts. Stability and
// retention are separate policies negotiated through the graph.
//
// A plain Abstract result requires no release from its consumer. An operation
// transferring a release obligation returns Retained or a stronger contract
// that exposes the obligation.
typedef struct ttx_abstract {
  void* context;
  const struct ttx_abstract_ops* operations;
} ttx_abstract;

// Visitation delivers each route and Abstract synchronously to the callback.
// Route bytes and the Abstract remain accessible through that callback. The
// consumer copies any route bytes it keeps and negotiates any required retained
// access before the callback returns. A supplying contract may establish a
// longer access duration.
typedef struct ttx_graph_visitor {
  void* callback_context;
  void (*callback)(
      void* callback_context,
      perimortem_view_bytes route,
      ttx_abstract value);
} ttx_graph_visitor;

typedef struct ttx_abstract_ops {
  // Reports whether the provider establishes the UUID's semantic promise.
  // The provider returns Satisfied when it establishes that promise, Unknown
  // when it leaves the promise undetermined, and Rejected when it refuses it.
  // Obtaining the API requires a separate `bind` request, whose result can
  // differ as provider state changes. This call transfers no API record.
  //
  // Using a context pointer other than the pointer provided when `supports` was
  // acquired is undefined behavior.
  ttx_negotiation_status (*supports)(void* context, perimortem_uuid contract);

  // Supplies the complete C API record identified by the UUID. The caller
  // provides writable output storage of that contract's exact type, size and
  // alignment. The established Abstract ABI and UUID determine the record
  // layout and callable signatures. The provider may construct the record
  // from stored, generated or delegated state.
  //
  // The consumer uses the API after `bind` returns Satisfied. A call returning
  // Unknown or Rejected leaves the output unusable. The consumer controls
  // the output record, while each referenced context, description and
  // function follows its supplying access contract. Copying the record
  // preserves those access durations.
  // On a successful bind, the returned API satisfies the requested contract.
  // Any further binding is a separate request to that API's provider.
  //
  // The caller uses the context supplied with this operation. Substituting
  // another context through this interface is undefined behavior.
  ttx_negotiation_status (
      *bind)(void* context, perimortem_uuid contract, void* output);

  // Resolves a navigation request using this Abstract's policy. The provider
  // defines the route's byte format and supplies the complete returned
  // Abstract. A route describes this request, while an edge describes a graph
  // relationship. Further navigation uses the returned Abstract's operations.
  // Binding requests its typed contracts separately.
  ttx_abstract (*resolve_route)(void* context, perimortem_view_bytes route);

  // Visitation enumerates the routes visible through this Abstract's policy
  // and their Abstracts. Each callback preserves the traversed state until it
  // returns. Visitation order acquires meaning only through a separately
  // negotiated policy.
  void (*visit_routes)(void* context, ttx_graph_visitor visitor);

  // Classifies this Abstract as None, Unknown or Present in one provider call.
  // The caller uses the context supplied with this table. The provider reports
  // one of these three states through its own interpretation of that context.
  //
  // For an unchanged result, `supports` and `bind` must both agree with its
  // graph state. A None result must satisfy the None UUID and reject Unknown.
  // An Unknown result must satisfy the Unknown UUID and reject None. A
  // Present node must reject both reserved UUIDs. Requests for other UUIDs
  // remain independent. Present has no reserved contract UUID.
  ttx_abstract_state (*get_state)(void* context);
} ttx_abstract_ops;

C_LINKAGE
EXPORTED(TTX) const ttx_representation* ttx_abstract_representation(void);

#endif
