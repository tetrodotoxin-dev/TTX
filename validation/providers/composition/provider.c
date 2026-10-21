// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/providers/composition/provider.h"

#include <assert.h>
#include <string.h>

#include "toolchain/export.h"

#include "ttx/data/transfer.h"
#include "ttx/graph/policies/constant.h"
#include "ttx/graph/policies/none.h"

static const ttx_abstract_ops abstract_operations;
static const ttx_retained_ops retained_operations;
static const ttx_retain_ops retain_operations;
static const expression_ops expression_operations;
static const retained_expression_ops qualified_operations;

// Retained interfaces share one reference count. This assertion catches use
// after their final release while discovery remains available.
static const composition_subject* checked(composition_subject* source) {
  const composition_subject* subject = source;
  assert(!subject->acquired || subject->state->references);
  return subject;
}

static U8 is(perimortem_uuid id, U64 high, U64 low) {
  return id.high == high && id.low == low;
}

static ttx_abstract abstract_view(composition_subject* source) {
  return (ttx_abstract){source, &abstract_operations};
}

static ttx_abstract_state graph_state(void* context) {
  (void)context;
  return TTX_ABSTRACT_PRESENT;
}

static ttx_negotiation_status supports(void* context, perimortem_uuid id) {
  if (id.high == TTX_NONE_ID_HIGH &&
      (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
    return TTX_NEGOTIATION_REJECTED;
  }

  composition_subject* source = context;
  const composition_subject* subject = checked(source);
  if (subject->state->refuse_bindings) {
    return TTX_NEGOTIATION_REJECTED;
  }

  if (is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW) ||
      is(id, TTX_RETAIN_ID_HIGH, TTX_RETAIN_ID_LOW) ||
      is(id, COMPOSITION_ID_HIGH, EXPRESSION_ID_LOW)) {
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (is(id, COMPOSITION_ID_HIGH, ADDITIONAL_ID_LOW)) {
    return subject->state->additional;
  }

  // Retained and Constant apply to the acquired expression. Discovery supports
  // evaluation and Retain, while this branch requires an existing acquisition.
  if (subject->acquired) {
    if (is(id, TTX_RETAINED_ID_HIGH, TTX_RETAINED_ID_LOW) ||
        is(id, TTX_CONSTANT_ID_HIGH, TTX_CONSTANT_ID_LOW)) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (is(id, COMPOSITION_ID_HIGH, RETAINED_EXPRESSION_ID_LOW)) {
      return subject->state->qualified ? TTX_NEGOTIATION_SATISFIED
                                       : TTX_NEGOTIATION_REJECTED;
    }
  }

  return TTX_NEGOTIATION_UNKNOWN;
}

// These named operations remain callable through the supplied tables while
// HIDDEN keeps them out of the library's exported interface.
HIDDEN ttx_negotiation_status
    composition_bind(void* context, perimortem_uuid id, void* output) {
  composition_subject* source = context;
  const composition_subject* subject = checked(source);
  composition_state* state = subject->state;
  ++state->binds;

  const ttx_negotiation_status status = supports(source, id);
  if (status != TTX_NEGOTIATION_SATISFIED) {
    return status;
  }

  // Each bound operation publishes the context it interprets. Its supplied
  // get_abstract operation returns to this provider's graph location.
  if (is(id, TTX_RETAIN_ID_HIGH, TTX_RETAIN_ID_LOW)) {
    ttx_retain api = {
      subject->acquired ? &state->retained_expression : &state->weak_expression,
      &retain_operations};

    ttx_retain* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (is(id, TTX_RETAINED_ID_HIGH, TTX_RETAINED_ID_LOW)) {
    ttx_retained api = {source, &retained_operations};
    ttx_retained* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (is(id, COMPOSITION_ID_HIGH, EXPRESSION_ID_LOW)) {
    expression api = {
      subject->acquired ? &state->retained_expression : &state->weak_expression,
      &expression_operations};

    expression* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (is(id, COMPOSITION_ID_HIGH, RETAINED_EXPRESSION_ID_LOW)) {
    retained_expression api = {source, &qualified_operations};
    retained_expression* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  ttx_abstract api = abstract_view(source);
  ttx_abstract* result = output;
  *result = api;
  return TTX_NEGOTIATION_SATISFIED;
}

static ttx_abstract get_abstract(void* context) {
  composition_subject* source = context;
  checked(source);
  return abstract_view(source);
}

static ttx_abstract lookup(void* context, perimortem_view_bytes route) {
  composition_subject* source = context;
  checked(source);
  return route.size == 4 && memcmp(route.data, "self", 4) == 0
             ? abstract_view(source)
             : ttx_none();
}

static void visit(void* context, ttx_graph_visitor callback) {
  composition_subject* source = context;
  const composition_subject* subject = checked(source);
  ++subject->state->visits;
  callback.callback(
      callback.callback_context, (perimortem_view_bytes){(const U8*)"self", 4},
      abstract_view(source));
}

HIDDEN U64 composition_evaluate(void* context) {
  composition_subject* source = context;
  const composition_subject* subject = checked(source);
  return subject->acquired ? subject->state->snapshot : subject->state->value;
}

static void release(void* context) {
  composition_subject* source = context;
  const composition_subject* subject = checked(source);
  assert(subject->acquired);
  --subject->state->references;
  ++subject->state->releases;
}

static ttx_negotiation_status retain(void* context, ttx_retained* output) {
  composition_subject* source = context;
  composition_state* state = checked(source)->state;

  // Overlapping retains share one snapshot. Capture again only after every
  // earlier retain has been released.
  if (!state->references) {
    state->snapshot = state->value;
  }

  ++state->references;
  ++state->acquisitions;
  *output = (ttx_retained){&state->retained, &retained_operations};
  return TTX_NEGOTIATION_SATISFIED;
}

static const ttx_abstract_ops abstract_operations = {
  supports, composition_bind, lookup, visit, graph_state};

static const ttx_retained_ops retained_operations = {get_abstract, release};

// The typed context has a guard before its graph reference. Treating it as a
// composition_subject would read the wrong state. Reentry selects a separate
// graph record that represents the same location and governing acquisition.
static composition_subject* expression_graph(void* context) {
  composition_expression* value = context;
  assert(value->guard == 0x174295836abcef01ULL);
  checked(value->graph);
  return value->graph;
}

static U64 expression_evaluate(void* context) {
  return composition_evaluate(expression_graph(context));
}

static ttx_abstract expression_abstract(void* context) {
  return abstract_view(expression_graph(context));
}

static ttx_abstract expression_other(void* context) {
  expression_graph(context);
  return ttx_none();
}

static ttx_negotiation_status expression_retain(
    void* context,
    ttx_retained* output) {
  return retain(expression_graph(context), output);
}

static const ttx_retain_ops retain_operations = {
  expression_abstract, expression_retain};

static const expression_ops expression_operations = {
  expression_evaluate, expression_abstract, expression_other};

static ttx_retained get_retained(void* context) {
  composition_subject* source = context;
  checked(source);
  return (ttx_retained){source, &retained_operations};
}

static const retained_expression_ops qualified_operations = {
  composition_evaluate, get_retained};

C_LINKAGE EXPORTED(TTX_TEST)
ttx_abstract composition_open(composition_state* state) {
  state->weak = (composition_subject){state, 0};
  state->retained = (composition_subject){state, 1};
  state->weak_returned = (composition_subject){state, 0};
  state->retained_returned = (composition_subject){state, 1};
  state->weak_expression =
      (composition_expression){0x174295836abcef01ULL, &state->weak_returned};
  state->retained_expression = (composition_expression){
    0x174295836abcef01ULL, &state->retained_returned};

  return abstract_view(&state->weak);
}

C_LINKAGE EXPORTED(TTX_TEST)
ttx_negotiation_status composition_acquire(
    composition_state* state,
    retained_expression* output) {
  if (!state->qualified) {
    return TTX_NEGOTIATION_REJECTED;
  }

  ttx_retained acquired;
  retain(&state->weak, &acquired);
  *output = (retained_expression){acquired.context, &qualified_operations};
  return TTX_NEGOTIATION_SATISFIED;
}
