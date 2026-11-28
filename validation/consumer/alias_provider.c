// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/alias.h"
#include "ttx/graph/policies/none.h"
#include "validation/consumer/alias.h"

// The C provider publishes the first export until a consumer redirects it to
// the authorized replacement. A binding receives only the two export records.
// The graph operations use the enclosing location as their context.
typedef struct {
  ttx_abstract current;
  ttx_abstract replacement;
} exports;

typedef struct {
  ttx_abstract location;
  exports targets;
} reference;

static ttx_abstract get(void* context) {
  return ((const exports*)context)->current;
}

static ttx_negotiation_status set(void* context, ttx_abstract target) {
  exports* targets = context;
  if (target.context != targets->replacement.context ||
      target.operations != targets->replacement.operations) {
    return TTX_NEGOTIATION_REJECTED;
  }

  targets->current = target;
  return TTX_NEGOTIATION_SATISFIED;
}

static ttx_abstract resolve(ttx_abstract self) {
  const ttx_abstract current =
      ((const reference*)self.context)->targets.current;
  return current.operations->resolve(current);
}

static ttx_negotiation_status supports(void* context, perimortem_uuid id) {
  (void)context;
  if ((id.high == TTX_ALIAS_ID_HIGH && id.low == TTX_ALIAS_ID_LOW) ||
      (id.high == TTX_ABSTRACT_ID_HIGH && id.low == TTX_ABSTRACT_ID_LOW)) {
    return TTX_NEGOTIATION_SATISFIED;
  }

  return TTX_NEGOTIATION_REJECTED;
}

static ttx_negotiation_status
    bind(void* context, perimortem_uuid id, void* output) {
  static const ttx_alias_ops alias = {get, set};
  reference* slot = context;
  if (id.high == TTX_ALIAS_ID_HIGH && id.low == TTX_ALIAS_ID_LOW) {
    *(ttx_alias*)output = (ttx_alias){&slot->targets, &alias};
    return TTX_NEGOTIATION_SATISFIED;
  }

  if (id.high == TTX_ABSTRACT_ID_HIGH && id.low == TTX_ABSTRACT_ID_LOW) {
    *(ttx_abstract*)output = slot->location;
    return TTX_NEGOTIATION_SATISFIED;
  }

  return TTX_NEGOTIATION_REJECTED;
}

static ttx_abstract route(void* context, perimortem_view_bytes name) {
  (void)context;
  (void)name;
  return ttx_none();
}

static void visit(void* context, ttx_graph_visitor visitor) {
  (void)context;
  (void)visitor;
}

static ttx_abstract_reachability reachable(void* context) {
  (void)context;
  return TTX_ABSTRACT_REACHABLE;
}

S32 validate_c_provider(ttx_abstract initial, ttx_abstract replacement) {
  static const ttx_abstract_ops operations = {supports, bind,      route,
                                              visit,    reachable, resolve};

  reference slot = {{&slot, &operations}, {initial, replacement}};
  return consume_c_provider(slot.location, initial, replacement);
}
