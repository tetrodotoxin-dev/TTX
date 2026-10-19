// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/providers/retention/provider.h"

#include <assert.h>
#include <stdlib.h>

#include "ttx/concept/capabilities/borrow.h"
#include "ttx/concept/capabilities/callable.h"
#include "ttx/concept/policies/none.h"
#include "ttx/concept/policies/unknown.h"
#include "ttx/data/transfer.h"
#include "ttx/semantic/realization/invocation.h"
#include "validation/providers/representation.h"

static const perimortem_uuid increment_id = {
  0x75a9a5de1b164e82ULL, 0x823e42db676680acULL};

static retention_statistics observed;
static ttx_binding_status creation_status, borrowing_status;
static const ttx_representation* integer_form;
static const ttx_borrow_ops temporary_operations, retained_borrow_operations;
static const ttx_borrowed_ops retained_operations;
static const ttx_callable_operations callable_operations;
static const ttx_create_ops factory_operations;

// The creation callback receives a stack value. Borrow substitutes separately
// allocated state with a different layout. Its operations interpret that new
// context through the same requested contract.
typedef struct temporary_value {
  U64 initial;
} temporary_value;

typedef struct retained_value {
  Count references;
  U64 value;
} retained_value;

static int is(perimortem_uuid id, U64 high, U64 low) {
  return id.high == high && id.low == low;
}

static perimortem_view_bytes data(void* context) {
  (void)context;
  return (perimortem_view_bytes){0};
}

static ttx_abstract lookup(void* context, perimortem_view_bytes route) {
  (void)context;
  (void)route;
  return ttx_none();
}

static void visit(void* context, ttx_concept_visitor callback) {
  (void)context;
  (void)callback;
}

static ttx_abstract temporary_abstract(void* context) {
  return (ttx_abstract){context, &temporary_operations.abstract};
}

static ttx_binding_status temporary_supports(
    void* context,
    perimortem_uuid id) {
  (void)context;
  return is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW) ||
                 is(id, TTX_BORROW_ID_HIGH, TTX_BORROW_ID_LOW)
             ? TTX_BINDING_SATISFIED
             : TTX_BINDING_UNKNOWN;
}

static ttx_binding_status
    temporary_bind(void* context, perimortem_uuid id, ttx_storage target) {
  if (is(id, TTX_BORROW_ID_HIGH, TTX_BORROW_ID_LOW)) {
    ttx_borrow api = {context, &temporary_operations};
    const ttx_storage input = {
      ttx_borrow_representation(), (U8*)&api, sizeof(api)};

    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  if (is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW)) {
    ttx_abstract api = temporary_abstract(context);
    const ttx_storage input = {
      ttx_abstract_representation(), (U8*)&api, sizeof(api)};

    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  return TTX_BINDING_UNKNOWN;
}

static ttx_abstract retained_abstract(void* context) {
  return (ttx_abstract){context, &retained_operations.abstract};
}

static ttx_binding_status retained_supports(void* context, perimortem_uuid id) {
  (void)context;
  return temporary_supports(context, id) == TTX_BINDING_SATISFIED ||
                 is(id, TTX_BORROWED_ID_HIGH, TTX_BORROWED_ID_LOW) ||
                 is(id, TTX_CALLABLE_ID_HIGH, TTX_CALLABLE_ID_LOW) ||
                 is(id, increment_id.high, increment_id.low)
             ? TTX_BINDING_SATISFIED
             : TTX_BINDING_UNKNOWN;
}

// This symbol is hidden from lookup. The consumer obtains it only as part of
// a negotiated API record, and repeated calls observe changing provider state.
HIDDEN ttx_data_status
    retention_invoke(void* context, const void* input, void* output) {
  retained_value* value = context;
  const U64* amount = input;
  U64* result = output;
  value->value += *amount;
  *result = value->value;
  ++observed.calls;
  return TTX_DATA_SUCCESS;
}

static ttx_binding_status
    retained_bind(void* context, perimortem_uuid id, ttx_storage target) {
  if (is(id, TTX_BORROW_ID_HIGH, TTX_BORROW_ID_LOW)) {
    ttx_borrow api = {context, &retained_borrow_operations};
    const ttx_storage input = {
      ttx_borrow_representation(), (U8*)&api, sizeof(api)};

    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  if (is(id, TTX_BORROWED_ID_HIGH, TTX_BORROWED_ID_LOW)) {
    ttx_borrowed api = {context, &retained_operations};
    const ttx_storage input = {
      ttx_borrowed_representation(), (U8*)&api, sizeof(api)};

    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  if (is(id, TTX_CALLABLE_ID_HIGH, TTX_CALLABLE_ID_LOW)) {
    ttx_callable api = {context, &callable_operations};
    const ttx_storage input = {
      ttx_callable_representation(), (U8*)&api, sizeof(api)};

    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  if (is(id, increment_id.high, increment_id.low)) {
    ttx_invocation api = {
      context, integer_form, integer_form, retention_invoke};

    const ttx_storage input = {
      ttx_invocation_representation(), (U8*)&api, sizeof(api)};

    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  if (is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW)) {
    ttx_abstract api = retained_abstract(context);
    const ttx_storage input = {
      ttx_abstract_representation(), (U8*)&api, sizeof(api)};

    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  return TTX_BINDING_UNKNOWN;
}

static ttx_binding_status acquire(void* context, ttx_borrowed* output) {
  retained_value* value = context;
  ++value->references;
  ++observed.acquisitions;
  *output = (ttx_borrowed){value, &retained_operations};
  return TTX_BINDING_SATISFIED;
}

static ttx_binding_status borrow_temporary(
    void* context,
    ttx_borrowed* output) {
  if (borrowing_status != TTX_BINDING_SATISFIED) {
    return borrowing_status;
  }

  const temporary_value* initial = context;
  retained_value* value = malloc(sizeof(*value));
  assert(value);
  *value = (retained_value){0, initial->initial};
  ++observed.live;
  return acquire(value, output);
}

static void release(void* context) {
  retained_value* value = context;
  ++observed.releases;
  if (--value->references == 0) {
    --observed.live;
    free(value);
  }
}

static ttx_binding_status describe(
    void* context,
    ttx_callable_description* output) {
  (void)context;

  static ttx_callable_field field;
  field = (ttx_callable_field){ttx_unknown(), 0};
  *output = (ttx_callable_description){
    increment_id,
    ttx_invocation_representation(),
    {integer_form, &field, 1},
    {integer_form, &field, 1}};

  return TTX_BINDING_SATISFIED;
}

static ttx_abstract factory_abstract(void* context) {
  return (ttx_abstract){context, &factory_operations.abstract};
}

static ttx_binding_status factory_supports(void* context, perimortem_uuid id) {
  (void)context;
  return is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW) ||
                 is(id, TTX_CREATE_ID_HIGH, TTX_CREATE_ID_LOW)
             ? TTX_BINDING_SATISFIED
             : TTX_BINDING_UNKNOWN;
}

static ttx_binding_status
    factory_bind(void* context, perimortem_uuid id, ttx_storage target) {
  if (is(id, TTX_CREATE_ID_HIGH, TTX_CREATE_ID_LOW)) {
    ttx_create api = {context, &factory_operations};
    const ttx_storage input = {
      ttx_create_representation(), (U8*)&api, sizeof(api)};

    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  if (is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW)) {
    ttx_abstract api = factory_abstract(context);
    const ttx_storage input = {
      ttx_abstract_representation(), (U8*)&api, sizeof(api)};

    return ttx_transfer(&input, &target) == TTX_DATA_SUCCESS
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  return TTX_BINDING_UNKNOWN;
}

static ttx_binding_status create(
    void* context,
    ttx_abstract arguments,
    void* callback_context,
    void (*callback)(void*, ttx_abstract)) {
  (void)context;
  (void)arguments;
  if (creation_status != TTX_BINDING_SATISFIED) {
    return creation_status;
  }

  temporary_value initial = {41};
  observed.inside_callback = 1;
  ++observed.callbacks;
  callback(callback_context, temporary_abstract(&initial));
  observed.inside_callback = 0;
  return TTX_BINDING_SATISFIED;
}

static const ttx_borrow_ops temporary_operations = {
  {temporary_supports, temporary_bind, data, temporary_abstract, lookup, visit},
  borrow_temporary};

static const ttx_borrowed_ops retained_operations = {
  {retained_supports, retained_bind, data, retained_abstract, lookup, visit},
  release};

static const ttx_borrow_ops retained_borrow_operations = {
  {retained_supports, retained_bind, data, retained_abstract, lookup, visit},
  acquire};

static const ttx_callable_operations callable_operations = {
  {retained_supports, retained_bind, data, retained_abstract, lookup, visit},
  describe};

static const ttx_create_ops factory_operations = {
  {factory_supports, factory_bind, data, factory_abstract, lookup, visit},
  create};

static retention_statistics statistics(void) {
  return observed;
}

static void configure(
    ttx_binding_status creation,
    ttx_binding_status borrowing) {
  assert(observed.live == 0);
  observed = (retention_statistics){0};
  creation_status = creation;
  borrowing_status = borrowing;
}

retention_fixture retention_provider_open(void) {
  static const ttx_schema integer = {
    8,
    8,
    TTX_SCHEMA_VALUE,
    {.value = {TTX_SCHEMA_U64, TTX_SCHEMA_LITTLE_ENDIAN}}};

  if (!integer_form) {
    compile_representation = ttx_representation_compile;
    integer_form = prepare_representation(&integer);
  }

  configure(TTX_BINDING_SATISFIED, TTX_BINDING_SATISFIED);
  return (retention_fixture){
    factory_abstract(&observed), statistics, configure};
}
