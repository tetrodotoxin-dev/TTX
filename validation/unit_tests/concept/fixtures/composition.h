// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef VALIDATION_CONCEPT_COMPOSITION_H
#define VALIDATION_CONCEPT_COMPOSITION_H
#include "ttx/concept/capabilities/borrow.h"

#define COMPOSITION_ID_HIGH 0x51e5f3f94e93471fULL
#define EXPRESSION_ID_LOW 0xbb18658950679f12ULL
#define BORROWED_EXPRESSION_ID_LOW 0xa9dd0343163941d9ULL
#define ADDITIONAL_ID_LOW 0xb14106da734161a3ULL

typedef struct expression {
  void* source;
  const struct expression_ops* operations;
} expression;
typedef struct expression_ops {
  ttx_abstract_ops abstract;
  U64 (*evaluate)(void*);
} expression_ops;
typedef struct borrowed_expression {
  void* source;
  const struct borrowed_expression_ops* operations;
} borrowed_expression;
typedef struct borrowed_expression_ops {
  ttx_borrowed_ops borrowed;
  U64 (*evaluate)(void*);
} borrowed_expression_ops;

typedef struct composition_subject {
  struct composition_state* state;
  U8 acquired;
} composition_subject;
typedef struct composition_forms {
  const ttx_representation* abstract;
  const ttx_representation* borrow;
  const ttx_representation* borrowed;
  const ttx_representation* expression;
  const ttx_representation* qualified;
} composition_forms;
typedef struct composition_state {
  composition_subject weak;
  composition_subject retained;
  composition_forms forms;
  U64 value, snapshot, references, acquisitions, releases, binds, visits;
  ttx_binding_status additional;
  U8 qualified;
} composition_state;

typedef ttx_abstract (
    *composition_open_function)(composition_state*, composition_forms);
typedef ttx_binding_status (
    *composition_acquire_function)(composition_state*, borrowed_expression*);
#endif
