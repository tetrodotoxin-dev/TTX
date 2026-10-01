// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_NEGOTIATION_QUERY_H
#define TTX_SEMANTIC_NEGOTIATION_QUERY_H

#include "perimortem/system/uuid.h"

#include "ttx/semantic/negotiation/binding.h"

// Two systems need an agreed entry point before either can ask the other for
// an interface. A module can return this Query from its entry function, or a
// native provider can lend it directly. That enclosing agreement supplies the
// `bind` thunk and its lifetime, so calling `bind` does not first require a
// Flow. Systems can therefore begin cooperating without constructing a TTX
// graph. Once that first Query is available, `bind` can acquire interfaces that
// support further negotiation, such as an Abstract exposed by a plugin.
//
// A caller sometimes needs to know what a subject promises without acquiring
// any operations. `supports` answers that semantic question using only the
// UUID. For example, an unsigned policy can exclude negative values without
// exposing a callable interface. The answer transfers no bytes and requires no
// agreed Representation. Satisfied establishes that promise, Unknown leaves the
// question unsettled, and Rejected records an explicit refusal. An unfamiliar
// UUID therefore need not be rejected. These answers are observations, with
// no implied background work or obligation to become determined later.
//
// `bind` asks the stronger question: can this provider supply the promised API
// in the concrete form the caller can consume? Success populates that admitted
// Storage. `supports` may succeed while every requested binding is rejected,
// since a shared semantic promise does not repair an incompatible API format.
// A support answer therefore grants no permission to call or cast anything.
// Callers needing operations can bind directly without a preliminary probe.
//
// The canonical descriptor builds off of the TTX::Data protocol and includes
// callable signatures along with their calling conventions, so this exchange
// checks how to call the supplied API as well as how to store it. This is why
// a Query provider using the consumer's native calling convention simplifies
// bootstrapping. It establishes how to call `bind` before negotiating other
// interfaces. Providers using other calling conventions remain useful once
// that initial agreement exists. Marker bindings still agree on an empty API,
// while `supports` avoids that representation exchange altogether. Binding's
// status and borrowing rules are described in
// ttx/semantic/negotiation/binding.h.
//
// `source` refers to private provider state. A consumer accessing that state
// through Query is undefined behavior under TTX. The provider may substitute
// a different implementation with the same semantic answers, so the consumer
// uses the supplied operations and lifetime agreement rather than assuming a
// native object layout.
//
// The supplying protocol keeps the provider state and implementation code
// alive through negotiation and every use of the returned bindings. Copying
// this Query borrows that agreement without acquiring another lifetime.
typedef struct ttx_semantic_query {
  void* source;
  ttx_binding_status (*bind)(
      void* source,
      perimortem_uuid contract,
      ttx_storage requested);
  ttx_binding_status (*supports)(void* source, perimortem_uuid contract);
} ttx_semantic_query;

#endif
