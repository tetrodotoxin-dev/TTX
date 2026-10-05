// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/providers/composition/provider.h"

#include <assert.h>
#include <string.h>

#include "toolchain/export.h"
#include "ttx/concept/policies/constant.h"
#include "ttx/concept/policies/none.h"

static const ttx_abstract_ops abstract_operations;
static const ttx_borrowed_ops borrowed_operations;
static const ttx_borrow_ops borrow_operations;
static const expression_ops expression_operations;
static const borrowed_expression_ops qualified_operations;

// Retained interfaces share one reference count. This assertion catches use
// after their final release while discovery remains available.
static const composition_subject* checked(composition_subject* source) {
  const composition_subject* subject = source;
  assert(!subject->acquired || subject->state->references);
  return subject;
}

static int is(perimortem_uuid id, U64 high, U64 low) {
  return id.high == high && id.low == low;
}

static ttx_abstract abstract_view(composition_subject* source) {
  return (ttx_abstract){source, &abstract_operations};
}

static ttx_binding_status supports(void* context, perimortem_uuid id) {
  composition_subject* source = context;
  const composition_subject* subject = checked(source);
  if (is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW) ||
      is(id, TTX_BORROW_ID_HIGH, TTX_BORROW_ID_LOW) ||
      is(id, COMPOSITION_ID_HIGH, EXPRESSION_ID_LOW)) {
    return TTX_BINDING_SATISFIED;
  }

  if (is(id, COMPOSITION_ID_HIGH, ADDITIONAL_ID_LOW)) {
    return subject->state->additional;
  }

  // Acquisition adds the retained contracts. Discovery still provides
  // evaluation and the capability to request a borrow.
  if (subject->acquired) {
    if (is(id, TTX_BORROWED_ID_HIGH, TTX_BORROWED_ID_LOW) ||
        is(id, TTX_CONSTANT_ID_HIGH, TTX_CONSTANT_ID_LOW)) {
      return TTX_BINDING_SATISFIED;
    }

    if (is(id, COMPOSITION_ID_HIGH, BORROWED_EXPRESSION_ID_LOW)) {
      return subject->state->qualified ? TTX_BINDING_SATISFIED
                                       : TTX_BINDING_REJECTED;
    }
  }

  return TTX_BINDING_UNKNOWN;
}

// These named operations remain callable through the supplied tables while
// HIDDEN keeps them out of the library's exported interface.
HIDDEN ttx_binding_status
    composition_bind(void* context, perimortem_uuid id, ttx_storage target) {
  composition_subject* source = context;
  const composition_subject* subject = checked(source);
  composition_state* state = subject->state;
  ++state->binds;

  const ttx_binding_status status = supports(source, id);
  if (status != TTX_BINDING_SATISFIED) {
    return status;
  }

  // Each bound record keeps this context and bind function, so subsequent
  // questions continue through the policy that admitted the record.
  if (is(id, TTX_BORROW_ID_HIGH, TTX_BORROW_ID_LOW)) {
    const ttx_borrow api = {source, &borrow_operations};
    return ttx_binding_provide(state->forms.borrow, &api, target);
  }

  if (is(id, TTX_BORROWED_ID_HIGH, TTX_BORROWED_ID_LOW)) {
    const ttx_borrowed api = {source, &borrowed_operations};
    return ttx_binding_provide(state->forms.borrowed, &api, target);
  }

  if (is(id, COMPOSITION_ID_HIGH, EXPRESSION_ID_LOW)) {
    const expression api = {source, &expression_operations};
    return ttx_binding_provide(state->forms.expression, &api, target);
  }

  if (is(id, COMPOSITION_ID_HIGH, BORROWED_EXPRESSION_ID_LOW)) {
    const borrowed_expression api = {source, &qualified_operations};
    return ttx_binding_provide(state->forms.qualified, &api, target);
  }

  const ttx_abstract api = abstract_view(source);
  return ttx_binding_provide(state->forms.abstract, &api, target);
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

static ttx_binding_status borrow(void* context, ttx_borrowed* output) {
  composition_subject* source = context;
  composition_state* state = checked(source)->state;

  // Overlapping borrows share one snapshot. Capture again only after every
  // earlier borrow has been released.
  if (!state->references) {
    state->snapshot = state->value;
  }

  ++state->references;
  ++state->acquisitions;
  *output = (ttx_borrowed){&state->retained, &borrowed_operations};
  return TTX_BINDING_SATISFIED;
}

static const ttx_abstract_ops abstract_operations = {
  supports, composition_bind, data, resolve, lookup, visit};

static const ttx_borrowed_ops borrowed_operations = {
  {supports, composition_bind, data, resolve, lookup, visit},
  release};

static const ttx_borrow_ops borrow_operations = {
  {supports, composition_bind, data, resolve, lookup, visit},
  borrow};

static const expression_ops expression_operations = {
  composition_evaluate,
  {supports, composition_bind, data, resolve, lookup, visit}};

static const borrowed_expression_ops qualified_operations = {
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
ttx_binding_status composition_acquire(
    composition_state* state,
    borrowed_expression* output) {
  if (!state->qualified) {
    return TTX_BINDING_REJECTED;
  }

  ttx_borrowed acquired;
  borrow(&state->weak, &acquired);
  *output = (borrowed_expression){acquired.context, &qualified_operations};
  return TTX_BINDING_SATISFIED;
}
