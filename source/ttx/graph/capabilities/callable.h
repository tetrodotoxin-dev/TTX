// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_CAPABILITIES_CALLABLE_H
#define TTX_GRAPH_CAPABILITIES_CALLABLE_H

#include "ttx/data/form/representation.h"
#include "ttx/graph/abstract.h"

#define TTX_CALLABLE_ID_HIGH 0x84c6c053b3254f7aULL
#define TTX_CALLABLE_ID_LOW 0xa873409b31bcf605ULL

// Describes one argument or result and its byte offset in the call frame.
// The Abstract explains how to interpret that value. For example, Boolean and
// integer arguments can use the same storage while exposing different
// contracts.
typedef struct ttx_callable_field {
  ttx_abstract abstract;
  Count offset;
} ttx_callable_field;

// A frame describes the layout of an operation's arguments or results. The
// provider supplies a readable canonical Representation and an array of `count`
// fields in argument order. Each field gives the meaning and byte offset of
// one value in that layout. With a zero count, the fields pointer is unused.
typedef struct ttx_callable_frame {
  const ttx_representation* representation;
  const ttx_callable_field* fields;
  Count count;
} ttx_callable_frame;

// Names an operation and describes the API record used to call it. The consumer
// selects an API it can call, compares that API's Representation with api, then
// binds contract using its required Representation. Input and output frames
// give the storage layouts and Abstracts describing each value's meaning.
// Fields follow argument order, with offsets locating their bytes in the frame.
//
// The provider supplies readable canonical API and frame Representations,
// valid field arrays and usable Abstracts through the description's access
// duration. Copying the description preserves the referenced storage's existing
// lifetime. A consumer keeping a field's Abstract establishes any required
// retention through that Abstract's capabilities.
typedef struct ttx_callable_description {
  perimortem_uuid contract;
  const ttx_representation* api;
  ttx_callable_frame inputs;
  ttx_callable_frame outputs;
} ttx_callable_description;

// Exposes the capability to describe an operation that another provider can
// supply. `describe` fills output and returns Satisfied when it can supply that
// description. Unknown leaves the request undetermined. Rejected explicitly
// refuses it. The caller consumes output only when `describe` returns
// Satisfied.
typedef struct ttx_callable_operations {
  ttx_abstract_ops abstract;
  ttx_negotiation_status (
      *describe)(void* context, ttx_callable_description* output);
} ttx_callable_operations;

typedef struct ttx_callable {
  void* context;
  const ttx_callable_operations* operations;
} ttx_callable;

C_LINKAGE EXPORTED(TTX) const ttx_representation* ttx_callable_representation(
    void);

#endif
