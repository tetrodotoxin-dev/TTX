// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef VALIDATION_GRAPH_COMPOSITION_H
#define VALIDATION_GRAPH_COMPOSITION_H

#include "ttx/graph/capabilities/retain.h"

#define COMPOSITION_ID_HIGH 0x51e5f3f94e93471fULL
#define EXPRESSION_ID_LOW 0xbb18658950679f12ULL
#define RETAINED_EXPRESSION_ID_LOW 0xb7130a1c896242e5ULL
#define ADDITIONAL_ID_LOW 0xb14106da734161a3ULL

typedef struct expression {
  void* context;
  const struct expression_ops* operations;
} expression;

typedef struct expression_ops {
  U64 (*evaluate)(void*);
  ttx_abstract (*get_abstract)(void* context);
  ttx_abstract (*other)(void* context);
} expression_ops;

typedef struct retained_expression {
  void* context;
  const struct retained_expression_ops* operations;
} retained_expression;

typedef struct retained_expression_ops {
  U64 (*evaluate)(void*);
  ttx_retained (*get_retained)(void* context);
} retained_expression_ops;

typedef struct composition_subject {
  struct composition_state* state;
  U8 acquired;
} composition_subject;

// The typed binding uses a different layout and context from its graph node.
// Only the expression operations interpret this record.
typedef struct composition_expression {
  U64 guard;
  composition_subject* graph;
} composition_expression;

typedef struct composition_state {
  composition_subject weak;
  composition_subject retained;
  composition_subject weak_returned;
  composition_subject retained_returned;
  composition_expression weak_expression;
  composition_expression retained_expression;
  U64 value, snapshot, references, acquisitions, releases, binds, visits;
  ttx_negotiation_status additional;
  U8 qualified;
  U8 refuse_bindings;
} composition_state;

typedef ttx_abstract (*composition_open_function)(composition_state*);
typedef ttx_negotiation_status (
    *composition_acquire_function)(composition_state*, retained_expression*);
#endif
