// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/providers/composition/provider.h"

#include <assert.h>
#include <string.h>

#include "toolchain/export.h"

#include "ttx/graph/policies/constant.h"
#include "ttx/graph/policies/none.h"

static const ttx_abstract_ops abstract_operations;
static const expression_ops expression_operations;

static U8 is(perimortem_uuid id, U64 high, U64 low) {
  return id.high == high && id.low == low;
}

static ttx_abstract graph(composition_subject* subject) {
  return (ttx_abstract){subject, &abstract_operations};
}

static ttx_abstract_state state(void* context) {
  (void)context;
  return TTX_ABSTRACT_PRESENT;
}

static ttx_negotiation_status supports(void* context, perimortem_uuid id) {
  composition_subject* subject = context;
  if (subject->state->refuse_bindings) {
    return TTX_NEGOTIATION_REJECTED;
  }

  if (!id.high && id.low <= 1) {
    return TTX_NEGOTIATION_REJECTED;
  }

  if (is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW) ||
      is(id, TTX_CONSTANT_ID_HIGH, TTX_CONSTANT_ID_LOW) ||
      is(id, COMPOSITION_ID_HIGH, EXPRESSION_ID_LOW)) {
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (is(id, COMPOSITION_ID_HIGH, ADDITIONAL_ID_LOW)) {
    return subject->state->additional;
  }

  return TTX_NEGOTIATION_UNKNOWN;
}

HIDDEN ttx_negotiation_status
    composition_bind(void* context, perimortem_uuid id, void* output) {
  composition_subject* subject = context;
  composition_state* source = subject->state;
  ++source->binds;

  const ttx_negotiation_status status = supports(context, id);
  if (status != TTX_NEGOTIATION_SATISFIED) {
    return status;
  }

  if (is(id, COMPOSITION_ID_HIGH, EXPRESSION_ID_LOW)) {
    *(expression*)output =
        (expression){&source->expression, &expression_operations};
  } else {
    *(ttx_abstract*)output = graph(subject);
  }

  return TTX_NEGOTIATION_SATISFIED;
}

static ttx_abstract route(void* context, perimortem_view_bytes requested) {
  composition_subject* subject = context;
  return requested.size == 4 && memcmp(requested.data, "self", 4) == 0
             ? graph(subject)
             : ttx_none();
}

static void visit(void* context, ttx_graph_visitor visitor) {
  composition_subject* subject = context;
  ++subject->state->visits;
  visitor.callback(
      visitor.callback_context, (perimortem_view_bytes){(const U8*)"self", 4},
      graph(subject));
}

static const ttx_abstract_ops abstract_operations = {
  supports, composition_bind, route, visit, state,
};

static composition_subject* expression_graph(void* context) {
  composition_expression* value = context;
  assert(value->guard == 0x174295836abcef01ULL);
  return value->graph;
}

HIDDEN U64 composition_evaluate(void* context) {
  return expression_graph(context)->state->value;
}

static ttx_abstract expression_abstract(void* context) {
  return graph(expression_graph(context));
}

static ttx_abstract expression_other(void* context) {
  (void)expression_graph(context);
  return ttx_none();
}

static const expression_ops expression_operations = {
  composition_evaluate,
  expression_abstract,
  expression_other,
};

C_LINKAGE EXPORTED(TTX_TEST)
ttx_abstract composition_open(composition_state* source) {
  source->origin = (composition_subject){source};
  source->returned = (composition_subject){source};
  source->expression =
      (composition_expression){0x174295836abcef01ULL, &source->returned};
  return graph(&source->origin);
}
