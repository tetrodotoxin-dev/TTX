// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_ABSTRACT_H
#define TTX_CONCEPT_ABSTRACT_H

#include "toolchain/toolchain.h"

#include "perimortem/core/view/bytes.h"
#include "perimortem/serialization/formats/uuid.h"

#include "ttx/data/form/storage.h"
#include "ttx/semantic/negotiation/status.h"

#define TTX_ABSTRACT_ID_HIGH 0x01a08522d86b7decULL
#define TTX_ABSTRACT_ID_LOW 0x9fa65e9c70720964ULL

// Abstract lets consumers navigate a semantic graph without knowing the
// native types of its nodes. Each provider interprets its routes and supplies
// the next Abstract. Traversal can cross domains with different native
// languages and storage models using only the Abstract operations at each step.
//
// Observations are Abstract-to-Abstract graph operations: `resolve`,
// `resolve_concept` and the Abstracts delivered by `visit_concepts`. To enter
// a typed API, the consumer calls `bind` with its contract UUID.
//
//   Abstract --observation--> Abstract --bind(UUID)--> Cursor
//   Cursor   --terminal-----> Token
//   Token    --operation----> Abstract (any graph location)
//
// A successful bind supplies the complete requested contract, including every
// required operation and policy. The guarantee applies to that returned API.
// Each further binding is a separate request whose success depends on the
// provider supplying that API.
//
// A terminal is an operation on known types, such as a Cursor returning a
// Token. Its contract governs those typed values rather than graph traversal.
// A typed operation may return an Abstract at any graph location. Observations
// resume from that supplied Abstract. Any relationship to the earlier graph
// location requires an explicit contract.
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
// The supplying call or retained interface keeps the context and table
// accessible through use. Copying an Abstract preserves that access duration.
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
typedef struct ttx_concept_visitor {
  void* context;
  void (*callback)(
      void* context,
      perimortem_view_bytes route,
      ttx_abstract value);
} ttx_concept_visitor;

typedef struct ttx_abstract_ops {
  // Provides a path to quickly check if an Abstract adheres to the requested
  // semantic policy or capability contract. Accessing a possible API still
  // requires a separate `bind` call with a negotiated representation. That bind
  // can then return Rejected if the provider does not supply the API in the
  // requested form. `supports` also does not promise that any such form exists.
  //
  // Using a context pointer other than the pointer provided when `supports` was
  // acquired is undefined behavior.
  ttx_negotiation_status (*supports)(void* context, perimortem_uuid contract);

  // Transfers the complete API for the UUID into the caller's destination.
  // The caller supplies Storage with a checked canonical Representation and
  // sufficient capacity and alignment. The provider agrees its API with that
  // Representation, including callable signatures and conventions, before
  // transferring stored, generated or delegated data.
  //
  // The consumer uses the API after `bind` returns Satisfied. A call returning
  // Unknown or Rejected leaves the destination unusable. The consumer controls
  // the destination bytes, while each referenced context, description and
  // function follows its supplying access contract. Copying the record
  // preserves those access durations.
  // On a successful bind, the returned API satisfies the requested contract.
  // Any further binding is a separate request to that API's provider.
  //
  // The caller uses the context supplied with this operation. Substituting
  // another context through this interface is undefined behavior.
  ttx_negotiation_status (
      *bind)(void* context, perimortem_uuid contract, ttx_storage requested);

  // Direct byte access is a typed read supplied by the Abstract contract.
  // The bytes have no implied encoding or Constant promise, even when another
  // edge on this Abstract is Constant. The view survives until the next byte
  // read or graph observation on this Abstract, or the end of the supplying
  // lifetime. Keeping it longer requires the enclosing contract's stronger
  // lifetime or a copy made by the consumer.
  perimortem_view_bytes (*get_data)(void* context);

  // Returns the Abstract represented by this observation. The provider decides
  // which domain layers resolution removes and may return the current view.
  // For an unchanged observation state, resolving the returned view again
  // returns that same view. Equality identifies the context and operation
  // table, not equivalent semantic content or a shared C++ value.
  ttx_abstract (*resolve)(void* context);

  // Traverses one route using this Abstract's policy and returns the complete
  // Abstract selected by the provider. Route bytes have the provider's meaning.
  // Consumers can continue traversing the result before requesting a UUID
  // contract through its bind operation.
  ttx_abstract (*resolve_concept)(void* context, perimortem_view_bytes route);

  // Visitation enumerates the routes visible through this Abstract's policy
  // and their Abstracts. Each callback preserves the traversed state until it
  // returns. Visitation order acquires meaning only through a separately
  // negotiated policy.
  void (*visit_concepts)(void* context, ttx_concept_visitor visitor);
} ttx_abstract_ops;

C_LINKAGE
EXPORTED(TTX) const ttx_representation* ttx_abstract_representation(void);

#endif
