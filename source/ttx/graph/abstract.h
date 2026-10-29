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
// The Semantic Graph has two axiomatic results: None and Unknown. None marks an
// explicit dead end. Its provider commits that the selected path remains
// blocked for the supplying access duration. Unknown leaves the result
// unsettled. Retrying the original Abstract may produce another result, though
// Unknown promises neither scheduling nor eventual resolution.
//
// None and Unknown return themselves for every further route and produce no
// ordinary visitation callbacks. This preserves the result without exposing
// where it entered a route chain. Present identifies an ordinary graph location
// even when the consumer recognizes none of its contracts.
//
// Policies and Capabilities provide two useful logical categories for Abstract
// contracts. Policy support establishes a property of the Abstract, while a
// Capability can be bound to obtain an extensible API surface. These categories
// organize contracts rather than restricting their design.
//
// None and Unknown expose the axiomatic results as reserved Policies. Binding
// either Policy supplies its Abstract API. None satisfies Constant and rejects
// unrelated contract UUIDs. Unknown leaves Constant and unrelated contract
// UUIDs Unknown. Each rejects the other axiomatic Policy. `get_state` provides
// their direct classification and must agree with support and binding for the
// reserved UUIDs.
//
// `supports` observes whether the provider establishes a contract's semantic
// promise. `bind` independently observes whether the provider can materialize
// the API selected by that contract UUID. Neither result implies the other. A
// supported contract can remain Unknown to `bind`, while a contract Unknown to
// `supports` can still bind successfully.
//
// Graph Unknown is an Abstract result. Negotiation Unknown leaves a support or
// binding request unsettled. A successful graph observation can return None or
// Unknown without making the observation itself a failure.
//
// Route observations stay within Graph, while binding enters typed operations.
//
//   Abstract --observation--> Abstract --bind(UUID)--> Type
//   Type     --terminal-----> Type
//   Type     --operation----> Abstract (any graph location)
//
// The graph provides transactional access through observations. Each Abstract
// API defines the results committed by one observation. An observation commits
// all of those results or none of them. A failed observation commits no
// callbacks, but the provider can still change graph state while attempting it.
// Retrying therefore _always_ begins a new transaction.
//
// Idempotence applies only to the selected graph location and only when the
// observation's Abstract result satisfies policy support for
// `Graph::Policies::Constant`. None always supports Constant while Unknown
// intentionally leaves it unsettled. Typed data and outgoing observations
// remain governed by their own contracts.
//
// `resolve_route` returns one observation directly. Ordinary visitation
// performs one observation for each visible route, so every callback can
// observe a different graph state. `Graph::Capabilities::Selection` can
// establish one observation over its complete callback set when the target
// Abstract supplies that capability.
//
// A successful bind supplies the complete requested contract, including every
// required operation and policy. The guarantee applies to that returned API.
// Each further binding is a separate request whose success depends on the
// provider supplying that API. The provider can refuse later requests,
// including the bound contract's own UUID or Abstract, while the operations
// already supplied remain usable.
//
// A typed operation can return an Abstract anywhere in the graph. The optional
// `get_abstract` contract provides a way back after a successful bind. It
// returns an Abstract at the same graph location that supplied the binding,
// even when the returned API uses another context and operation table.
//
//   A --bind(Type)--> Type --get_abstract()--> A'
//
// A and A' represent the same graph location. This is semantic equivalence
// rather than API-record equality. Returning through `get_abstract` preserves
// the location and its Policies. Each API keeps its own published context and
// operations, and each later binding remains an independent request.
//
// The supplying contract establishes the complete API representation before
// the consumer calls its operations. That representation includes the table's
// C calling conventions and recursive Abstract results. A usable Abstract has
// a nonnull context, a readable operation table and every operation populated.
// Each API is valid only as its published construction of context and
// operations. The supplied functions interpret that context's private layout.
// A provider can publish several interfaces over the same backing state with
// different contexts or operations. Consumers preserve each supplied pairing.
//
// The supplying contract defines how long the context and operation table
// remain accessible. Copies of the Abstract share that duration, and graph
// observations do not extend it. Stability follows the Policies supplied by
// that Abstract.
//
// A plain Abstract result requires no release from its consumer. Stateful
// resources cross the graph boundary through typed contracts that define their
// own access and release obligations.
typedef struct ttx_abstract {
  void* context;
  const struct ttx_abstract_ops* operations;
} ttx_abstract;

// Visitation invokes the consumer's callback for each visible route. Every
// callback is an independent observation of the Abstract selected for that
// route. The callback can make further graph observations before the provider
// continues, and provider state can change before the next callback. Consumers
// requiring one transaction across several results can bind Selection when the
// provider supplies it.
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
