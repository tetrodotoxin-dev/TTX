// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_TEST_CONVERSION_H
#define TTX_TEST_CONVERSION_H

#include "ttx/graph/capabilities/export.h"
#include "ttx/graph/capabilities/import.h"

typedef struct conversion_output {
  Count artifacts;
  U8 value;
  U8 report_route;
  ttx_negotiation_status status;
} conversion_output;

#ifdef __cplusplus

extern "C" {

#endif

ttx_negotiation_status conversion_expose(
    conversion_output* output,
    ttx_abstract subject);
ttx_negotiation_status conversion_run(
    ttx_import importer,
    const void* input,
    const ttx_representation* representation,
    ttx_export exporter);

#ifdef __cplusplus
}

#endif
#endif
