// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_CAPABILITIES_CALLABLE_H
#define TTX_CONCEPT_CAPABILITIES_CALLABLE_H

#include "ttx/concept/abstract.h"
#include "ttx/data/form/representation.h"

#define TTX_CALLABLE_ID_HIGH 0x84c6c053b3254f7aULL
#define TTX_CALLABLE_ID_LOW 0xa873409b31bcf605ULL

// Describes one argument or result and its byte offset in the call frame.
// The Abstract explains how to interpret that value. For example, Boolean and
// integer arguments can use the same storage while exposing different
// contracts.
typedef struct ttx_callable_field {
  ttx_abstract subject;
  Count offset;
} ttx_callable_field;

typedef struct ttx_callable_frame {
  const ttx_representation* representation;
  const ttx_callable_field* fields;
  Count count;
} ttx_callable_frame;

// Names an operation and describes the API record used to call it. The caller
// binds contract using the Representation in api. Input and output frames give
// the storage layouts and the Abstracts that describe each value's meaning.
// Fields follow argument order, with offsets locating their bytes in the frame.
//
// Copying the description copies these records. Their Representations, field
// arrays and Abstracts remain borrowed from the supplying observation. A
// consumer keeping those observations copies their data or negotiates the
// retention it needs.
typedef struct ttx_callable_description {
  perimortem_uuid contract;
  const ttx_representation* api;
  ttx_callable_frame inputs;
  ttx_callable_frame outputs;
} ttx_callable_description;

// Exposes the capability to describe an operation that another object can
// supply. `describe` fills output and returns Satisfied when it can supply that
// description. Unknown leaves the request undetermined. Rejected explicitly
// refuses it. The caller consumes output only when `describe` returns
// Satisfied.
typedef struct ttx_callable_operations {
  ttx_abstract_ops abstract;
  ttx_binding_status (
      *describe)(void* source, ttx_callable_description* output);
} ttx_callable_operations;

typedef struct ttx_callable {
  void* source;
  const ttx_callable_operations* operations;
} ttx_callable;

C_LINKAGE const ttx_representation* ttx_callable_representation(void);

#endif
