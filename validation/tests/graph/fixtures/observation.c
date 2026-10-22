// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/tests/graph/fixtures/observation.h"

#include "ttx/graph/policies/constant.h"
#include "ttx/graph/policies/none.h"
#include "validation/tests/graph/fixtures/value.h"

static ttx_abstract_state graph_state(void* context) {
  (void)context;
  return TTX_ABSTRACT_PRESENT;
}

static U8 read(void* context);

static ttx_negotiation_status
    bind(void* context, perimortem_uuid id, void* output) {
  if (id.high == TTX_NONE_ID_HIGH &&
      (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
    return TTX_NEGOTIATION_REJECTED;
  }

  observation_subject* subject = context;
  ++subject->binds;
  if (subject->answer != TTX_NEGOTIATION_SATISFIED) {
    return subject->answer;
  }

  if (id.high == VALUE_ID_HIGH && id.low == VALUE_ID_LOW) {
    value_api* result = output;
    *result = (value_api){context, read};
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (id.high == TTX_ABSTRACT_ID_HIGH && id.low == TTX_ABSTRACT_ID_LOW) {
    ttx_abstract api = subject->selected.operations
                           ? subject->selected
                           : observation_abstract(subject);
    ttx_abstract* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (id.high == TTX_CONSTANT_ID_HIGH && id.low == TTX_CONSTANT_ID_LOW) {
    ttx_abstract api = observation_abstract(subject);
    ttx_abstract* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  return TTX_NEGOTIATION_UNKNOWN;
}

// Constant fixes the graph selection. Each typed read can still produce a
// new byte, whose value the consumer keeps independently of provider storage.
static U8 read(void* context) {
  observation_subject* subject = context;
  return ++subject->value;
}

static ttx_abstract get_abstract(void* context) {
  observation_subject* source = context;
  return observation_abstract(source);
}

static ttx_abstract lookup(void* context, perimortem_view_bytes route) {
  observation_subject* source = context;
  if (route.size == 3 && route.data[0] == 7 && route.data[1] == 0 &&
      route.data[2] == 9) {
    return get_abstract(source);
  }

  return ttx_none();
}

static void visit(void* context, ttx_graph_visitor visitor) {
  observation_subject* source = context;
  const U8 route[] = {
    7,
    0,
    9,
  };

  const perimortem_view_bytes bytes = {
    route,
    sizeof(route),
  };

  visitor.callback(visitor.callback_context, bytes, get_abstract(source));
}

static ttx_negotiation_status supports(void* context, perimortem_uuid id) {
  if (id.high == TTX_NONE_ID_HIGH &&
      (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
    return TTX_NEGOTIATION_REJECTED;
  }

  (void)context;
  return (id.high == VALUE_ID_HIGH && id.low == VALUE_ID_LOW) ||
                 (id.high == TTX_ABSTRACT_ID_HIGH &&
                  id.low == TTX_ABSTRACT_ID_LOW) ||
                 (id.high == TTX_CONSTANT_ID_HIGH &&
                  id.low == TTX_CONSTANT_ID_LOW)
             ? TTX_NEGOTIATION_SATISFIED
             : TTX_NEGOTIATION_UNKNOWN;
}

ttx_abstract observation_abstract(observation_subject* subject) {
  static const ttx_abstract_ops operations = {
    supports, bind, lookup, visit, graph_state,
  };

  const ttx_abstract result = {
    subject,
    &operations,
  };

  return result;
}
