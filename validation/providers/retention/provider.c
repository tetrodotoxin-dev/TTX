// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/providers/retention/provider.h"

#include <assert.h>
#include <stdlib.h>

#include "ttx/data/transfer.h"
#include "ttx/graph/capabilities/callable.h"
#include "ttx/graph/capabilities/retain.h"
#include "ttx/graph/policies/none.h"
#include "ttx/graph/policies/unknown.h"
#include "ttx/semantic/realization/invocation.h"
#include "validation/providers/representation.h"

static const perimortem_uuid increment_id = {
  0x75a9a5de1b164e82ULL, 0x823e42db676680acULL};

static retention_statistics observed;
static ttx_negotiation_status creation_status, retention_status;
static ttx_release_status cleanup_status;
static const ttx_representation* integer_form;
static const ttx_retain_ops temporary_operations, retained_retain_operations;
static const ttx_retained_ops retained_operations;
static const ttx_callable_operations callable_operations;
static const ttx_create_ops factory_operations;
static const ttx_abstract_ops temporary_graph_operations,
    retained_graph_operations, factory_graph_operations;

// The creation callback receives a temporary_value on the stack. `retain`
// copies its value into an allocated retained_value with a reference count.
// The returned operations use that retained_value as their context.
typedef struct temporary_value {
  U64 initial;
} temporary_value;

typedef struct retained_value {
  Count references;
  U64 value;
} retained_value;

static U8 is(perimortem_uuid id, U64 high, U64 low) {
  return id.high == high && id.low == low;
}

static ttx_abstract_state graph_state(void* context) {
  (void)context;
  return TTX_ABSTRACT_PRESENT;
}

static ttx_abstract lookup(void* context, perimortem_view_bytes route) {
  (void)context;
  (void)route;
  return ttx_none();
}

static void visit(void* context, ttx_graph_visitor callback) {
  (void)context;
  (void)callback;
}

static ttx_abstract temporary_abstract(void* context) {
  return (ttx_abstract){context, &temporary_graph_operations};
}

static ttx_negotiation_status temporary_supports(
    void* context,
    perimortem_uuid id) {
  if (id.high == TTX_NONE_ID_HIGH &&
      (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
    return TTX_NEGOTIATION_REJECTED;
  }

  (void)context;
  return is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW) ||
                 is(id, TTX_RETAIN_ID_HIGH, TTX_RETAIN_ID_LOW)
             ? TTX_NEGOTIATION_SATISFIED
             : TTX_NEGOTIATION_UNKNOWN;
}

static ttx_negotiation_status
    temporary_bind(void* context, perimortem_uuid id, void* output) {
  if (id.high == TTX_NONE_ID_HIGH &&
      (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
    return TTX_NEGOTIATION_REJECTED;
  }

  if (is(id, TTX_RETAIN_ID_HIGH, TTX_RETAIN_ID_LOW)) {
    ttx_retain api = {context, &temporary_operations};
    ttx_retain* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW)) {
    ttx_abstract api = temporary_abstract(context);
    ttx_abstract* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  return TTX_NEGOTIATION_UNKNOWN;
}

static ttx_abstract retained_abstract(void* context) {
  return (ttx_abstract){context, &retained_graph_operations};
}

static ttx_negotiation_status retained_supports(
    void* context,
    perimortem_uuid id) {
  if (id.high == TTX_NONE_ID_HIGH &&
      (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
    return TTX_NEGOTIATION_REJECTED;
  }

  (void)context;
  return temporary_supports(context, id) == TTX_NEGOTIATION_SATISFIED ||
                 is(id, TTX_RETAINED_ID_HIGH, TTX_RETAINED_ID_LOW) ||
                 is(id, TTX_CALLABLE_ID_HIGH, TTX_CALLABLE_ID_LOW) ||
                 is(id, increment_id.high, increment_id.low)
             ? TTX_NEGOTIATION_SATISFIED
             : TTX_NEGOTIATION_UNKNOWN;
}

// The consumer obtains this function through a negotiated API, even though
// symbol lookup cannot find it. Each call increments the retained value, so
// the test also checks that a bound operation observes changes in provider
// state.
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

static ttx_negotiation_status
    retained_bind(void* context, perimortem_uuid id, void* output) {
  if (id.high == TTX_NONE_ID_HIGH &&
      (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
    return TTX_NEGOTIATION_REJECTED;
  }

  if (is(id, TTX_RETAIN_ID_HIGH, TTX_RETAIN_ID_LOW)) {
    ttx_retain api = {context, &retained_retain_operations};
    ttx_retain* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (is(id, TTX_RETAINED_ID_HIGH, TTX_RETAINED_ID_LOW)) {
    ttx_retained api = {context, &retained_operations};
    ttx_retained* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (is(id, TTX_CALLABLE_ID_HIGH, TTX_CALLABLE_ID_LOW)) {
    ttx_callable api = {context, &callable_operations};
    ttx_callable* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (is(id, increment_id.high, increment_id.low)) {
    ttx_invocation api = {
      context, integer_form, integer_form, retention_invoke};

    ttx_invocation* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW)) {
    ttx_abstract api = retained_abstract(context);
    ttx_abstract* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  return TTX_NEGOTIATION_UNKNOWN;
}

static ttx_negotiation_status acquire(void* context, ttx_retained* output) {
  retained_value* value = context;
  ++value->references;
  ++observed.acquisitions;
  *output = (ttx_retained){value, &retained_operations};
  return TTX_NEGOTIATION_SATISFIED;
}

static ttx_negotiation_status retain_temporary(
    void* context,
    ttx_retained* output) {
  if (retention_status != TTX_NEGOTIATION_SATISFIED) {
    return retention_status;
  }

  const temporary_value* initial = context;
  retained_value* value = malloc(sizeof(*value));
  assert(value);
  *value = (retained_value){0, initial->initial};
  ++observed.live;
  return acquire(value, output);
}

static ttx_release_status release(void* context) {
  retained_value* value = context;
  ++observed.releases;
  if (--value->references == 0) {
    const ttx_release_status status = cleanup_status;
    ++observed.cleanups;
    --observed.live;
    free(value);
    return status;
  }

  return TTX_RELEASE_SUCCESS;
}

static ttx_negotiation_status describe(
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

  return TTX_NEGOTIATION_SATISFIED;
}

static ttx_abstract factory_abstract(void* context) {
  return (ttx_abstract){context, &factory_graph_operations};
}

static ttx_negotiation_status factory_supports(
    void* context,
    perimortem_uuid id) {
  if (id.high == TTX_NONE_ID_HIGH &&
      (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
    return TTX_NEGOTIATION_REJECTED;
  }

  (void)context;
  return is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW) ||
                 is(id, TTX_CREATE_ID_HIGH, TTX_CREATE_ID_LOW)
             ? TTX_NEGOTIATION_SATISFIED
             : TTX_NEGOTIATION_UNKNOWN;
}

static ttx_negotiation_status
    factory_bind(void* context, perimortem_uuid id, void* output) {
  if (id.high == TTX_NONE_ID_HIGH &&
      (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
    return TTX_NEGOTIATION_REJECTED;
  }

  if (is(id, TTX_CREATE_ID_HIGH, TTX_CREATE_ID_LOW)) {
    ttx_create api = {context, &factory_operations};
    ttx_create* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (is(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW)) {
    ttx_abstract api = factory_abstract(context);
    ttx_abstract* result = output;
    *result = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  return TTX_NEGOTIATION_UNKNOWN;
}

static ttx_negotiation_status create(
    void* context,
    ttx_abstract arguments,
    void* callback_context,
    void (*callback)(void*, ttx_abstract)) {
  (void)context;
  (void)arguments;
  if (creation_status != TTX_NEGOTIATION_SATISFIED) {
    return creation_status;
  }

  temporary_value initial = {41};
  observed.inside_callback = 1;
  ++observed.callbacks;
  callback(callback_context, temporary_abstract(&initial));
  observed.inside_callback = 0;
  return TTX_NEGOTIATION_SATISFIED;
}

static const ttx_abstract_ops temporary_graph_operations = {
  temporary_supports, temporary_bind, lookup, visit, graph_state};

static const ttx_abstract_ops retained_graph_operations = {
  retained_supports, retained_bind, lookup, visit, graph_state};

static const ttx_abstract_ops factory_graph_operations = {
  factory_supports, factory_bind, lookup, visit, graph_state};

static const ttx_retain_ops temporary_operations = {
  temporary_abstract, retain_temporary};

static const ttx_retained_ops retained_operations = {
  retained_abstract, release};

static const ttx_retain_ops retained_retain_operations = {
  retained_abstract, acquire};

static const ttx_callable_operations callable_operations = {
  retained_abstract, describe};

static const ttx_create_ops factory_operations = {factory_abstract, create};

static retention_statistics statistics(void) {
  return observed;
}

static void configure(
    ttx_negotiation_status creation,
    ttx_negotiation_status retention) {
  assert(observed.live == 0);
  observed = (retention_statistics){0};
  creation_status = creation;
  retention_status = retention;
  cleanup_status = TTX_RELEASE_SUCCESS;
}

// The simulated external cleanup reports its outcome after the provider's
// allocation is freed. Statistics stay in the fixture so the consumer can
// check completion without using any released interface.
static void configure_cleanup(ttx_release_status status) {
  assert(observed.live == 0);
  cleanup_status = status;
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

  configure(TTX_NEGOTIATION_SATISFIED, TTX_NEGOTIATION_SATISFIED);
  return (retention_fixture){
    factory_abstract(&observed), statistics, configure, configure_cleanup};
}
