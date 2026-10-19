// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_ABSTRACT_H
#define TTX_CONCEPT_ABSTRACT_H

#include "toolchain/toolchain.h"

#include "perimortem/core/view/bytes.h"
#include "perimortem/serialization/formats/uuid.h"

#include "ttx/semantic/negotiation/binding.h"

#define TTX_ABSTRACT_ID_HIGH 0x01a08522d86b7decULL
#define TTX_ABSTRACT_ID_LOW 0x9fa65e9c70720964ULL

// Abstract exposes a semantic graph through provider supplied operations.
// Consumers negotiate capabilities, observe bytes and traverse routes using
// the same interface across native languages and storage models. Each function
// interprets its supplied context, so providers can compose behavior without
// sharing a native base class or allocating an object for each graph edge.
//
// The supplying contract establishes the complete API representation before
// the consumer calls its operations. That representation includes the table's
// C calling conventions and recursive Abstract results. A usable Abstract has
// a nonnull context, a readable operation table and every operation populated.
// Its private context layout is interpreted by the supplied functions. Several
// interfaces can use the same context with different operations.
//
// The supplying call or retained interface keeps the context and table
// accessible through use. Copying an Abstract preserves that access duration.
// Later observations can return different data or routes. Stability and
// retention are separate policies negotiated through the graph.
//
// A plain Abstract result requires no release from its consumer. An operation
// transferring a release obligation returns Borrowed or a stronger contract
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
  ttx_binding_status (*supports)(void* context, perimortem_uuid contract);

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
  // Further negotiation through the bound API preserves the policies governing
  // this interface. Newly produced values carry their own contracts.
  //
  // The caller uses the context supplied with this operation. Substituting
  // another context through this interface is undefined behavior.
  ttx_binding_status (
      *bind)(void* context, perimortem_uuid contract, ttx_storage requested);

  // The data observation supplies bytes without another Abstract to inspect.
  // They have no implied encoding or Constant promise, even when another edge
  // on this Abstract is Constant. The view survives until the next observation
  // on this Abstract or the end of the supplying lifetime. Retaining it longer
  // requires the enclosing contract's stronger lifetime or a copy made by the
  // consumer.
  perimortem_view_bytes (*get_data)(void* context);

  // Returns the Abstract represented by this observation. The provider decides
  // which domain layers resolution removes and may return the current view.
  // For an unchanged observation state, resolving the returned view again
  // returns that same view. Equality identifies the context and operation
  // table, not equivalent semantic content or a shared C++ value.
  ttx_abstract (*resolve)(void* context);

  // Resolution and visitation use this Abstract's policy to select routes.
  // Visitation enumerates visible routes and their Abstracts. Each callback
  // preserves the traversed state until it returns. Visitation order acquires
  // meaning only through a separately negotiated policy.
  ttx_abstract (*resolve_concept)(void* context, perimortem_view_bytes route);
  void (*visit_concepts)(void* context, ttx_concept_visitor visitor);
} ttx_abstract_ops;

C_LINKAGE EXPORTED(TTX) const ttx_representation* ttx_abstract_representation(
    void);

#endif
