// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef VALIDATION_GRAPH_COMPOSITION_H
#define VALIDATION_GRAPH_COMPOSITION_H

#include "ttx/graph/abstract.h"

#define COMPOSITION_ID_HIGH 0x51e5f3f94e93471fULL
#define EXPRESSION_ID_LOW 0xbb18658950679f12ULL
#define ADDITIONAL_ID_LOW 0xb14106da734161a3ULL

typedef struct expression {
  void* context;
  const struct expression_ops* operations;
} expression;

typedef struct expression_ops {
  U64 (*evaluate)(void* context);
  ttx_abstract (*get_abstract)(void* context);
  ttx_abstract (*other)(void* context);
} expression_ops;

typedef struct composition_subject {
  struct composition_state* state;
} composition_subject;

// The typed binding uses a different layout and context from its graph node.
// Only the expression operations interpret this record.
typedef struct composition_expression {
  U64 guard;
  composition_subject* graph;
} composition_expression;

typedef struct composition_state {
  composition_subject origin;
  composition_subject returned;
  composition_expression expression;
  U64 value, binds, visits;
  ttx_negotiation_status additional;
  U8 refuse_bindings;
} composition_state;

typedef ttx_abstract (*composition_open_function)(composition_state* state);

#endif
