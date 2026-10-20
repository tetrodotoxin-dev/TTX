// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef VALIDATION_GRAPH_COMPOSITION_H
#define VALIDATION_GRAPH_COMPOSITION_H

#include "ttx/graph/capabilities/retain.h"

#define COMPOSITION_ID_HIGH 0x51e5f3f94e93471fULL
#define EXPRESSION_ID_LOW 0xbb18658950679f12ULL
#define RETAINED_EXPRESSION_ID_LOW 0xa9dd0343163941d9ULL
#define ADDITIONAL_ID_LOW 0xb14106da734161a3ULL

typedef struct expression {
  void* context;
  const struct expression_ops* operations;
} expression;

typedef struct expression_ops {
  U64 (*evaluate)(void*);
  ttx_abstract_ops abstract;
} expression_ops;

typedef struct retained_expression {
  void* context;
  const struct retained_expression_ops* operations;
} retained_expression;

typedef struct retained_expression_ops {
  U64 (*evaluate)(void*);
  ttx_retained_ops retained;
} retained_expression_ops;

typedef struct composition_subject {
  struct composition_state* state;
  U8 acquired;
} composition_subject;

typedef struct composition_forms {
  const ttx_representation* abstract;
  const ttx_representation* retain;
  const ttx_representation* retained;
  const ttx_representation* expression;
  const ttx_representation* qualified;
} composition_forms;

typedef struct composition_state {
  composition_subject weak;
  composition_subject retained;
  composition_forms forms;
  U64 value, snapshot, references, acquisitions, releases, binds, visits;
  ttx_negotiation_status additional;
  U8 qualified;
} composition_state;

typedef ttx_abstract (
    *composition_open_function)(composition_state*, composition_forms);
typedef ttx_negotiation_status (
    *composition_acquire_function)(composition_state*, retained_expression*);
#endif
