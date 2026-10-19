// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_ABSTRACT_H
#define TTX_CONCEPT_ABSTRACT_H

#include "perimortem/core/view/bytes.h"
#include "toolchain/toolchain.h"

#include "perimortem/serialization/formats/uuid.h"

#include "ttx/semantic/negotiation/binding.h"

#define TTX_ABSTRACT_ID_HIGH 0x01a08522d86b7decULL
#define TTX_ABSTRACT_ID_LOW 0x9fa65e9c70720964ULL

// A graph can contain providers that have no native C++ Abstract base. Keeping
// the context separate from its operations lets those providers answer from
// their own storage, without allocating an adapter object for every graph
// edge. Consumers traverse the supplied operations instead of depending on
// the language or implementation supplying the Abstract.
//
// The supplying boundary establishes the exact Abstract contract before the
// table is used, since an unknown table cannot safely describe how to call
// itself. That agreement includes capability queries, interface binding, byte
// observations, resolution and synchronous visitation. This representation uses
// the platform C ABI.
//
// This contract chooses one opaque context and one typed table pointer.
// Its canonical Representation includes the table's callable signatures and
// their recursive Abstract results. Agreement therefore checks more than the
// outer two pointer slots. The context's private layout remains unspecified.
//
// This record is an observation view. Its supplying call or retained provider
// keeps `context` and the operation table available while that view is used.
// Copying the record copies access, with lifetime still supplied by that scope.
// A later observation queries the provider again.
//
// Returning plain Abstract permits a caller to finish without releasing it.
// An acquired result must advertise its required lifetime policy in its return
// contract, since hiding a release obligation behind Abstract can leak data.
// `context` is nonnull and is interpreted by the supplied operations.
// Distinct interfaces can use the same context with different operations.
typedef struct ttx_abstract {
  void* context;
  const struct ttx_abstract_ops* operations;
} ttx_abstract;

// A provider enumerates its storage through synchronous callbacks. Each route
// and value is available during its callback. The supplying contract can
// establish longer access to a value; consumers obtain any required retention
// during the callback and copy route bytes they keep afterward.
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

  // Fills the caller's admitted destination with the complete API for contract
  // in the destination's representation. The representation defines the record
  // layout, including callable signatures and calling conventions. The provider
  // may select stored data, compose a record, or delegate its production.
  //
  // When `bind` returns Satisfied, the destination contains the complete API.
  // When it returns Unknown or Rejected, the destination contains no usable API.
  // The caller controls the destination bytes but does not thereby own any
  // context, data, descriptions, or executable code referenced by the record.
  // The contract supplying each reference defines how long it remains available.
  // Copying the record copies those pointers without extending that access.
  //
  // Using a context pointer other than the pointer provided when `bind` was
  // acquired is undefined behavior.
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

  // Lookup and discovery use the same Abstract's policy. Visitation advertises
  // the answers visible through that policy and gives their order no meaning.
  // The callback must not invalidate the traversed state during a callback.
  ttx_abstract (*resolve_concept)(void* context, perimortem_view_bytes route);
  void (*visit_concepts)(void* context, ttx_concept_visitor visitor);
} ttx_abstract_ops;

C_LINKAGE EXPORTED(TTX) const ttx_representation* ttx_abstract_representation(
    void);

#endif
