// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/consumer/alias.h"

#include "ttx/graph/policies/alias.h"
#include "ttx/graph/policies/none.h"
#include "ttx/graph/policies/promise.h"
#include "ttx/graph/policies/unknown.h"

// Both nodes publish the same single route. Each context supplies its own
// child, so a caller can distinguish the nodes by traversing the graph.
static ttx_abstract child(void* context, perimortem_view_bytes route) {
  if (route.size == 1 && route.data[0] == 'x') {
    return *(const ttx_abstract*)context;
  }

  return ttx_none();
}

static void visit(void* context, ttx_graph_visitor visitor) {
  const U8 name = 'x';
  const perimortem_view_bytes route = {&name, 1};
  visitor.callback(visitor.callback_context, route, child(context, route));
}

static ttx_negotiation_status supports(void* context, perimortem_uuid id) {
  (void)context;
  (void)id;
  return TTX_NEGOTIATION_REJECTED;
}

static ttx_abstract resolve(ttx_abstract self) {
  return *(const ttx_abstract*)self.context;
}

static ttx_negotiation_status
    bind(void* context, perimortem_uuid id, void* output) {
  (void)output;
  return supports(context, id);
}

static ttx_abstract_reachability reachable(void* context) {
  (void)context;
  return TTX_ABSTRACT_REACHABLE;
}

S32 validate_promise(ttx_abstract location, U8 pending) {
  const perimortem_uuid policy = {TTX_PROMISE_ID_HIGH, TTX_PROMISE_ID_LOW};
  const perimortem_uuid abstract = {TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW};
  const ttx_abstract selected = location.operations->resolve(location);
  ttx_abstract output = {0};
  if (pending) {
    return location.operations->supports(location.context, policy) ==
                       TTX_NEGOTIATION_SATISFIED &&
                   location.operations->bind(
                       location.context, policy, &output) ==
                       TTX_NEGOTIATION_UNKNOWN &&
                   selected.operations->get_reachability(selected.context) ==
                       TTX_ABSTRACT_INDETERMINATE
               ? 0
               : 15;
  }

  return location.operations->supports(location.context, policy) ==
                     TTX_NEGOTIATION_REJECTED &&
                 location.operations->bind(
                     location.context, abstract, &output) ==
                     TTX_NEGOTIATION_SATISFIED &&
                 output.operations->get_reachability(output.context) ==
                     TTX_ABSTRACT_UNREACHABLE &&
                 selected.operations->get_reachability(selected.context) ==
                     TTX_ABSTRACT_UNREACHABLE
             ? 0
             : 16;
}

S32 validate_alias(ttx_abstract location) {
  static const ttx_abstract_ops operations = {supports, bind,      child,
                                              visit,    reachable, resolve};

  ttx_abstract child_a = ttx_unknown();
  ttx_abstract child_b = ttx_none();
  const ttx_abstract a = {&child_a, &operations};
  const ttx_abstract b = {&child_b, &operations};

  const perimortem_uuid alias_id = {TTX_ALIAS_ID_HIGH, TTX_ALIAS_ID_LOW};
  ttx_alias writer;
  if (location.operations->bind(location.context, alias_id, &writer) !=
      TTX_NEGOTIATION_SATISFIED) {
    return 1;
  }

  if (writer.operations->set_referent(writer.context, ttx_none()) !=
          TTX_NEGOTIATION_SATISFIED ||
      location.operations->resolve(location).operations !=
          ttx_none().operations ||
      writer.operations->set_referent(writer.context, a) !=
          TTX_NEGOTIATION_SATISFIED) {
    return 2;
  }

  const ttx_abstract captured = writer.operations->get_referent(writer.context);
  const ttx_abstract original = location.operations->resolve(location);

  if (writer.operations->set_referent(writer.context, b) !=
      TTX_NEGOTIATION_SATISFIED) {
    return 3;
  }

  ttx_alias reader;
  if (location.operations->bind(location.context, alias_id, &reader) !=
      TTX_NEGOTIATION_SATISFIED) {
    return 4;
  }

  const ttx_abstract current = reader.operations->get_referent(reader.context);
  const U8 name = 'x';
  const perimortem_view_bytes route = {&name, 1};
  const ttx_abstract from_a =
      captured.operations->resolve_route(captured.context, route);
  const ttx_abstract from_b =
      current.operations->resolve_route(current.context, route);
  if (from_a.operations->get_reachability(from_a.context) !=
          TTX_ABSTRACT_INDETERMINATE ||
      from_b.operations->get_reachability(from_b.context) !=
          TTX_ABSTRACT_UNREACHABLE) {
    return 5;
  }

  const ttx_abstract redirected = location.operations->resolve(location);
  if (redirected.operations->get_reachability(redirected.context) !=
          TTX_ABSTRACT_UNREACHABLE ||
      original.operations->get_reachability(original.context) !=
          TTX_ABSTRACT_INDETERMINATE) {
    return 8;
  }

  if (writer.operations->set_referent(writer.context, ttx_unknown()) !=
      TTX_NEGOTIATION_SATISFIED) {
    return 9;
  }

  return validate_c_provider(a, b);
}
