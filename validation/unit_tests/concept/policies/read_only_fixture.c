// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/concept/policies/read_only_fixture.h"

#include <stdlib.h>

#include "ttx/concept/capabilities/borrow.h"
#include "ttx/concept/capabilities/create.h"
#include "ttx/concept/policies/read_only.h"
#include "ttx/concept/policies/unknown.h"

typedef struct provider {
  const ttx_representation* read;
  U32* freed;
  U32 references;
  U32 value;
} provider;

static int equal(perimortem_uuid id, U64 high, U64 low) {
  return id.high == high && id.low == low;
}
static ttx_binding_status supports(void*, perimortem_uuid);
static ttx_binding_status bind(void*, perimortem_uuid, ttx_storage);
static ttx_abstract resolve(void*);
static perimortem_view_bytes data(void* source) {
  const provider* state = source;
  const perimortem_view_bytes result = {
    (const U8*)&state->value, sizeof(state->value)};
  return result;
}
static ttx_abstract lookup(void* source, perimortem_view_bytes route) {
  if (route.size == 1 && route.data[0] == 'x') {
    return resolve(source);
  }
  return ttx_unknown();
}
static void visit(void* source, ttx_concept_visitor receive) {
  const perimortem_view_bytes route = {(const U8*)"x", 1};
  receive.receive(receive.source, route, resolve(source));
}
static const ttx_abstract_ops abstract_ops = {supports, bind,   data,
                                              resolve,  lookup, visit};
static ttx_abstract resolve(void* source) {
  const ttx_abstract result = {source, &abstract_ops};
  return result;
}
static void release(void* source) {
  provider* state = (provider*)source;
  if (!--state->references) {
    ++*state->freed;
    free(state);
  }
}
static ttx_binding_status acquired_supports(
    void* source,
    perimortem_uuid id) {
  if (equal(id, TTX_BORROWED_ID_HIGH, TTX_BORROWED_ID_LOW)) {
    return TTX_BINDING_SATISFIED;
  }
  return supports(source, id);
}
static ttx_binding_status
    acquired_bind(void* source, perimortem_uuid id, ttx_storage output);
static ttx_abstract acquired_resolve(void*);
static ttx_abstract acquired_lookup(
    void* source,
    perimortem_view_bytes route) {
  if (route.size == 1 && route.data[0] == 'x') {
    return acquired_resolve(source);
  }
  return ttx_unknown();
}
static void acquired_visit(void* source, ttx_concept_visitor receive) {
  const perimortem_view_bytes route = {(const U8*)"x", 1};
  receive.receive(receive.source, route, acquired_resolve(source));
}
static const ttx_abstract_ops acquired_ops = {
  acquired_supports, acquired_bind,   data,
  acquired_resolve,  acquired_lookup, acquired_visit};
static ttx_abstract acquired_resolve(void* source) {
  const ttx_abstract result = {source, &acquired_ops};
  return result;
}
static ttx_binding_status borrow(void* source, ttx_borrowed* output);
static const ttx_borrowed_ops retained_ops = {
  {acquired_supports, acquired_bind, data, acquired_resolve, acquired_lookup,
   acquired_visit},
  release};
static ttx_binding_status borrow(void* source, ttx_borrowed* output) {
  provider* state = (provider*)source;
  ++state->references;
  output->source = source;
  output->operations = &retained_ops;
  return TTX_BINDING_SATISFIED;
}
static U32 value(void* source) {
  return ((const provider*)source)->value;
}
static ttx_binding_status supports(void* source, perimortem_uuid id) {
  (void)source;
  if (equal(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW) ||
      equal(id, TTX_READ_ONLY_ID_HIGH, TTX_READ_ONLY_ID_LOW) ||
      equal(id, TEST_READ_ID_HIGH, TEST_READ_ID_LOW) ||
      equal(id, TTX_BORROW_ID_HIGH, TTX_BORROW_ID_LOW)) {
    return TTX_BINDING_SATISFIED;
  }
  if (equal(id, TTX_CREATE_ID_HIGH, TTX_CREATE_ID_LOW) ||
      equal(id, TTX_BORROWED_ID_HIGH, TTX_BORROWED_ID_LOW)) {
    return TTX_BINDING_REJECTED;
  }
  return TTX_BINDING_UNKNOWN;
}
static ttx_binding_status publish(
    void* source,
    perimortem_uuid id,
    ttx_storage target,
    int acquired) {
  const provider* state = source;
  const ttx_abstract_ops* ops = acquired ? &acquired_ops : &abstract_ops;
  if (equal(id, TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW) ||
      equal(id, TTX_READ_ONLY_ID_HIGH, TTX_READ_ONLY_ID_LOW)) {
    const ttx_abstract result = {source, ops};
    return ttx_binding_provide(ttx_abstract_representation(), &result, target);
  }
  if (equal(id, TEST_READ_ID_HIGH, TEST_READ_ID_LOW)) {
    static const test_read_ops weak = {
      {supports, bind, data, resolve, lookup, visit}, value};
    static const test_read_ops strong = {
      {acquired_supports, acquired_bind, data, acquired_resolve,
       acquired_lookup, acquired_visit},
      value};
    const test_read result = {source, acquired ? &strong : &weak};
    return ttx_binding_provide(state->read, &result, target);
  }
  if (equal(id, TTX_BORROW_ID_HIGH, TTX_BORROW_ID_LOW)) {
    static const ttx_borrow_ops weak = {
      {supports, bind, data, resolve, lookup, visit}, borrow};
    static const ttx_borrow_ops strong = {
      {acquired_supports, acquired_bind, data, acquired_resolve,
       acquired_lookup, acquired_visit},
      borrow};
    const ttx_borrow result = {source, acquired ? &strong : &weak};
    return ttx_binding_provide(ttx_borrow_representation(), &result, target);
  }
  if (acquired && equal(id, TTX_BORROWED_ID_HIGH, TTX_BORROWED_ID_LOW)) {
    const ttx_borrowed result = {source, &retained_ops};
    return ttx_binding_provide(ttx_borrowed_representation(), &result, target);
  }
  return supports(source, id);
}
static ttx_binding_status
    bind(void* source, perimortem_uuid id, ttx_storage target) {
  return publish(source, id, target, 0);
}
static ttx_binding_status
    acquired_bind(void* source, perimortem_uuid id, ttx_storage target) {
  return publish(source, id, target, 1);
}
void test_read_only_open(
    const ttx_representation* read,
    U32* freed,
    void* receiver,
    void (*receive)(void*, ttx_abstract)) {
  provider* state = malloc(sizeof(*state));
  if (!state) {
    abort();
  }
  state->read = read;
  state->freed = freed;
  state->references = 1;
  state->value = 42;
  receive(receiver, resolve(state));
  release(state);
}
