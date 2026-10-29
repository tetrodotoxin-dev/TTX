// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/tests/graph/fixtures/selection.h"

#include "ttx/graph/policies/none.h"
#include "ttx/graph/policies/ordered.h"
#include "validation/tests/graph/fixtures/value.h"

static ttx_abstract graph(void* context);

static ttx_negotiation_status select_values(
    void* context,
    ttx_abstract request,
    ttx_graph_visitor visitor) {
  selection_fixture* fixture = context;
  if (fixture->status != TTX_NEGOTIATION_SATISFIED) {
    return fixture->status;
  }

  value_api value;
  const perimortem_uuid value_id = {VALUE_ID_HIGH, VALUE_ID_LOW};
  if (request.operations->bind(request.context, value_id, &value) !=
      TTX_NEGOTIATION_SATISFIED) {
    return TTX_NEGOTIATION_UNKNOWN;
  }

  const U8 section = value.read(value.context);
  if (section == 0 || section > 2) {
    return TTX_NEGOTIATION_UNKNOWN;
  }

  if (section == 2 && !fixture->private_access) {
    return TTX_NEGOTIATION_SATISFIED;
  }

  static const U8 first[] = "first";
  static const U8 second[] = "second";
  const perimortem_view_bytes routes[] = {
    {first, sizeof(first) - 1},
    {second, sizeof(second) - 1},
  };

  const U64 count = section == 1 ? 2 : 1;
  for (U64 index = 0; index < count; ++index) {
    visitor.callback(visitor.callback_context, routes[index], ttx_none());
  }

  return TTX_NEGOTIATION_SATISFIED;
}

static ttx_negotiation_status selection_supports(
    void* context,
    perimortem_uuid requested) {
  selection_fixture* fixture = context;
  return requested.high == TTX_ORDERED_ID_HIGH &&
                 requested.low == TTX_ORDERED_ID_LOW
             ? fixture->ordering
             : TTX_NEGOTIATION_UNKNOWN;
}

static const ttx_selection_ops selection_operations = {
  selection_supports, select_values};

static ttx_negotiation_status supports(
    void* context,
    perimortem_uuid requested) {
  (void)context;
  if (!requested.high && requested.low <= 1) {
    return TTX_NEGOTIATION_REJECTED;
  }

  if ((requested.high == TTX_SELECTION_ID_HIGH &&
       requested.low == TTX_SELECTION_ID_LOW) ||
      (requested.high == TTX_ABSTRACT_ID_HIGH &&
       requested.low == TTX_ABSTRACT_ID_LOW)) {
    return TTX_NEGOTIATION_SATISFIED;
  }

  return TTX_NEGOTIATION_UNKNOWN;
}

static ttx_negotiation_status
    bind(void* context, perimortem_uuid requested, void* output) {
  const ttx_negotiation_status status = supports(context, requested);
  if (status != TTX_NEGOTIATION_SATISFIED) {
    return status;
  }

  if (requested.high == TTX_SELECTION_ID_HIGH &&
      requested.low == TTX_SELECTION_ID_LOW) {
    *(ttx_selection*)output = (ttx_selection){context, &selection_operations};
  } else {
    *(ttx_abstract*)output = graph(context);
  }

  return TTX_NEGOTIATION_SATISFIED;
}

static ttx_abstract route(void* context, perimortem_view_bytes requested) {
  (void)context;
  (void)requested;
  return ttx_none();
}

static void visit(void* context, ttx_graph_visitor visitor) {
  (void)context;
  (void)visitor;
}

static ttx_abstract_state state(void* context) {
  (void)context;
  return TTX_ABSTRACT_PRESENT;
}

static const ttx_abstract_ops graph_operations = {
  supports, bind, route, visit, state,
};

static ttx_abstract graph(void* context) {
  return (ttx_abstract){context, &graph_operations};
}

ttx_abstract selection_fixture_open(selection_fixture* fixture) {
  return graph(fixture);
}
