// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tests/concept/fixtures/conversion.h"

// This terminal copies a byte to its artifact storage. Reporting uses a
// second exporter supplied by the input graph, either on the root or a route.
// That service consumes the result while the imported graph is still available.
ttx_binding_status conversion_expose(
    conversion_output* output,
    ttx_abstract subject) {
  if (output->status != TTX_BINDING_SATISFIED) {
    return output->status;
  }
  const perimortem_view_bytes value =
      subject.operations->get_data(subject.source);
  if (value.size != 1) {
    return TTX_BINDING_REJECTED;
  }
  output->value = value.data[0];
  ++output->artifacts;

  ttx_abstract service = subject;
  if (output->report_route) {
    const U8 route[] = {'r', 'e', 'p', 'o', 'r', 't'};
    service = subject.operations->resolve_concept(
        subject.source, (perimortem_view_bytes){route, sizeof(route)});
  }
  ttx_export report = {0};
  const ttx_storage destination = {
    ttx_export_representation(), (U8*)&report, sizeof(report)};
  const perimortem_uuid id = {TTX_EXPORT_ID_HIGH, TTX_EXPORT_ID_LOW};
  if (service.operations->bind(service.source, id, destination) ==
      TTX_BINDING_SATISFIED) {
    // The reporting service is optional. Its refusal does not discard the
    // artifact already produced by this terminal.
    report.operations->expose(report.source, subject);
  }
  return TTX_BINDING_SATISFIED;
}

typedef struct conversion_visit {
  ttx_export exporter;
  ttx_binding_status status;
} conversion_visit;

static void receive(void* receiver, ttx_abstract subject) {
  conversion_visit* visit = receiver;
  visit->status =
      visit->exporter.operations->expose(visit->exporter.source, subject);
}

ttx_binding_status conversion_run(
    ttx_import importer,
    const void* input,
    const ttx_representation* representation,
    ttx_export exporter) {
  conversion_visit visit = {exporter, TTX_BINDING_UNKNOWN};
  const ttx_binding_status status = importer.operations->visit(
      importer.source, input, representation, &visit, receive);
  return status == TTX_BINDING_SATISFIED ? visit.status : status;
}
