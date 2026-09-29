// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_CAPABILITIES_EXPORT_H
#define TTX_CONCEPT_CAPABILITIES_EXPORT_H
#include "ttx/concept/abstract.h"

#define TTX_EXPORT_ID_HIGH 0x99ed754be007457fULL
#define TTX_EXPORT_ID_LOW 0x9b8e5b6d2fedbcc4ULL

// Uses an Abstract graph to produce a result in another system. The exporter
// selects the contracts and routes needed by its target. A graph can supply
// services alongside the content being exported. For example, a terminal can
// use a logger or report generated paths through a capability found on the
// root or one of its routes. Each participating contract defines those
// questions and whether their answers are required to produce the result.
//
// The graph is available through expose. Satisfied reports that the exporter
// fulfilled its request, Unknown leaves the outcome undetermined, and Rejected
// refuses it. The exporter defines its artifacts and effects, including any
// observations it reports through the supplied graph.
typedef struct ttx_export {
  const void* source;
  const struct ttx_export_ops* operations;
} ttx_export;

typedef struct ttx_export_ops {
  ttx_abstract_ops abstract;
  ttx_binding_status (*expose)(const void* source, ttx_abstract subject);
} ttx_export_ops;

PERIMORTEM_C const ttx_representation* ttx_export_representation(void);
#endif
