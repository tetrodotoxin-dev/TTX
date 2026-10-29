// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef VALIDATION_GRAPH_SELECTION_H
#define VALIDATION_GRAPH_SELECTION_H

#include "ttx/graph/capabilities/selection.h"

typedef struct selection_fixture {
  U8 private_access;
  ttx_negotiation_status status;
  ttx_negotiation_status ordering;
} selection_fixture;

C_LINKAGE ttx_abstract selection_fixture_open(selection_fixture* fixture);

#endif
