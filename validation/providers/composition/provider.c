// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/providers/composition/provider.h"

#include <assert.h>
#include <string.h>

#include "toolchain/export.h"

#include "ttx/concept/policies/constant.h"
#include "ttx/concept/policies/none.h"
#include "ttx/data/transfer.h"

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

static ttx_negotiation_status supports(void* context, perimortem_uuid id) {
  composition_subject* source = context;
  const composition_subject* subject = checked(source);
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
    composition_bind(void* context, perimortem_uuid id, ttx_storage target) {
  composition_subject* source = context;
  const composition_subject* subject = checked(source);
  composition_state* state = subject->state;
  ++state->binds;

  const ttx_negotiation_status status = supports(source, id);
  if (status != TTX_NEGOTIATION_SATISFIED) {
    return status;
  }

  // Each bound record keeps this context and bind function. Further
  // negotiation therefore uses the same governing policy.
  if (is(id, TTX_RETAIN_ID_HIGH, TTX_RETAIN_ID_LOW)) {
    ttx_retain api = {source, &retain_operations};
    const ttx_storage input = {state->forms.retain, (U8*)&api, sizeof(api)};
    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  if (is(id, TTX_RETAINED_ID_HIGH, TTX_RETAINED_ID_LOW)) {
    ttx_retained api = {source, &retained_operations};
    const ttx_storage input = {state->forms.retained, (U8*)&api, sizeof(api)};
    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  if (is(id, COMPOSITION_ID_HIGH, EXPRESSION_ID_LOW)) {
    expression api = {source, &expression_operations};
    const ttx_storage input = {state->forms.expression, (U8*)&api, sizeof(api)};
    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  if (is(id, COMPOSITION_ID_HIGH, RETAINED_EXPRESSION_ID_LOW)) {
    retained_expression api = {source, &qualified_operations};
    const ttx_storage input = {state->forms.qualified, (U8*)&api, sizeof(api)};
    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  ttx_abstract api = abstract_view(source);
  const ttx_storage input = {state->forms.abstract, (U8*)&api, sizeof(api)};
  return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
             ? TTX_NEGOTIATION_SATISFIED
             : TTX_NEGOTIATION_REJECTED;
}

static perimortem_view_bytes data(void* context) {
  composition_subject* source = context;
  const composition_subject* subject = checked(source);
  const U64* value =
      subject->acquired ? &subject->state->snapshot : &subject->state->value;
  return (perimortem_view_bytes){(const U8*)value, sizeof(*value)};
}

static ttx_abstract resolve(void* context) {
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

static void visit(void* context, ttx_concept_visitor callback) {
  composition_subject* source = context;
  const composition_subject* subject = checked(source);
  ++subject->state->visits;
  callback.callback(
      callback.context, (perimortem_view_bytes){(const U8*)"self", 4},
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
  supports, composition_bind, data, resolve, lookup, visit};

static const ttx_retained_ops retained_operations = {
  {supports, composition_bind, data, resolve, lookup, visit},
  release};

static const ttx_retain_ops retain_operations = {
  {supports, composition_bind, data, resolve, lookup, visit},
  retain};

static const expression_ops expression_operations = {
  composition_evaluate,
  {supports, composition_bind, data, resolve, lookup, visit}};

static const retained_expression_ops qualified_operations = {
  composition_evaluate,
  {{supports, composition_bind, data, resolve, lookup, visit}, release}};

C_LINKAGE EXPORTED(TTX_TEST)
ttx_abstract composition_open(
    composition_state* state,
    composition_forms forms) {
  state->weak = (composition_subject){state, 0};
  state->retained = (composition_subject){state, 1};
  state->forms = forms;
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
