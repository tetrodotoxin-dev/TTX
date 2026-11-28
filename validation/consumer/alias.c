// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/alias.h"

#include "ttx/graph/capabilities/normalize.h"
#include "ttx/graph/policies/none.h"
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
  if (id.high == TTX_NORMALIZE_ID_HIGH && id.low == TTX_NORMALIZE_ID_LOW) {
    return TTX_NEGOTIATION_UNKNOWN;
  }

  return TTX_NEGOTIATION_REJECTED;
}

// Each export stores its final subject as the context of resolve. Returning
// that record lets the test distinguish two exports that share an operation
// table but denote different subjects.
static ttx_negotiation_status resolve(void* context, ttx_abstract* output) {
  *output = *(const ttx_abstract*)context;
  return TTX_NEGOTIATION_SATISFIED;
}

static ttx_negotiation_status
    bind(void* context, perimortem_uuid id, void* output) {
  static const ttx_normalize_ops normalizer = {resolve};
  if (id.high == TTX_NORMALIZE_ID_HIGH && id.low == TTX_NORMALIZE_ID_LOW) {
    *(ttx_normalize*)output = (ttx_normalize){context, &normalizer};
    return TTX_NEGOTIATION_SATISFIED;
  }

  return supports(context, id);
}

static ttx_abstract_reachability reachable(void* context) {
  (void)context;
  return TTX_ABSTRACT_REACHABLE;
}

S32 main(void) {
  static const ttx_abstract_ops operations = {
    supports, bind, child, visit, reachable};

  ttx_abstract child_a = ttx_unknown();
  ttx_abstract child_b = ttx_none();
  const ttx_abstract a = {&child_a, &operations};
  const ttx_abstract b = {&child_b, &operations};

  const perimortem_uuid alias_id = {TTX_ALIAS_ID_HIGH, TTX_ALIAS_ID_LOW};
  ttx_alias_slot storage;
  const ttx_abstract location = ttx_alias_initialize(&storage);
  ttx_alias writer;
  if (location.operations->bind(location.context, alias_id, &writer) !=
      TTX_NEGOTIATION_SATISFIED) {
    return 1;
  }

  if (writer.operations->set_referent(writer.context, a) !=
      TTX_NEGOTIATION_SATISFIED) {
    return 2;
  }

  const ttx_abstract captured = writer.operations->get_referent(writer.context);
  const perimortem_uuid normalize_id = {
    TTX_NORMALIZE_ID_HIGH, TTX_NORMALIZE_ID_LOW};

  ttx_normalize normalizer;
  if (location.operations->bind(location.context, normalize_id, &normalizer) !=
      TTX_NEGOTIATION_SATISFIED) {
    return 6;
  }

  ttx_abstract original;
  if (normalizer.operations->resolve(normalizer.context, &original) !=
      TTX_NEGOTIATION_SATISFIED) {
    return 7;
  }

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

  ttx_abstract redirected;
  if (normalizer.operations->resolve(normalizer.context, &redirected) !=
          TTX_NEGOTIATION_SATISFIED ||
      redirected.operations->get_reachability(redirected.context) !=
          TTX_ABSTRACT_UNREACHABLE ||
      original.operations->get_reachability(original.context) !=
          TTX_ABSTRACT_INDETERMINATE) {
    return 8;
  }

  return 0;
}
