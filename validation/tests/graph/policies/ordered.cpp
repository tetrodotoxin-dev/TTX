// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/ordered.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Serialization;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;

static Harness Ordering = {.name = "TTX::Ordered"};

// Sequence publishes three occurrences, including the same route and Abstract
// twice. Visitation preserves those positions. Reordering the provider's model
// changes the sequence even when every occurrence keeps its route and value.
struct Sequence {
  struct Occurrence {
    View::Bytes route;
    ttx_abstract abstract;
  };

  Occurrence occurrences[3] = {
    {"repeat"_view, ttx_none()},
    {"middle"_view, ttx_unknown()},
    {"repeat"_view, ttx_none()},
  };

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    const Formats::Uuid contract(id);
    if (contract == Policies::None::contract_id ||
        contract == Policies::Unknown::contract_id) {
      return TTX_NEGOTIATION_REJECTED;
    }

    return contract == Abstract::contract_id ||
                   contract == Policies::Ordered::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    const auto status = supports(context, id);
    if (status != TTX_NEGOTIATION_SATISFIED) {
      return status;
    }

    *Data::cast<ttx_abstract>(output) = {context, &operations};
    return TTX_NEGOTIATION_SATISFIED;
  }

  static auto route(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    const auto& sequence = *Data::cast<Sequence>(context);
    for (const auto& occurrence : sequence.occurrences) {
      if (occurrence.route == View::Bytes(route)) {
        return occurrence.abstract;
      }
    }

    return ttx_none();
  }

  static auto visit(void* context, ttx_graph_visitor visitor) -> void {
    const auto& sequence = *Data::cast<Sequence>(context);
    for (const auto& occurrence : sequence.occurrences) {
      visitor.callback(
          visitor.callback_context, occurrence.route, occurrence.abstract);
    }
  }

  static auto state(void* context) -> ttx_abstract_state {
    return TTX_ABSTRACT_PRESENT;
  }

  static constexpr ttx_abstract_ops operations = {
    supports, bind, route, visit, state};
};

VALIDATION_TEST(Ordering, repeated_occurrences) {
  Sequence provider;
  const Abstract root(&provider, Sequence::operations);
  const auto bound = root.bind<Policies::Ordered>();
  ASSERT(bound);

  bound.visit(
      [&](Policies::Ordered ordered) {
        const auto graph = ordered.get_abstract();
        const View::Bytes routes[] = {
          "repeat"_view, "middle"_view, "repeat"_view};

        const Abstract::State states[] = {
          Abstract::State::None, Abstract::State::Unknown,
          Abstract::State::None};

        auto check = [&](const Count(&positions)[3]) {
          Count count = 0;
          auto callback = [&](View::Bytes route, Abstract value) {
            ASSERT(count < 3);

            const auto position = positions[count];
            EXPECT(route == routes[position]);
            EXPECT(value.get_state() == states[position]);
            EXPECT(graph.resolve_route(route) == value);
            ++count;
          };

          graph.visit_routes(Abstract::Visitor(callback));
          EXPECT_EQ(count, Count(3));
        };

        const Count original[] = {0, 1, 2};
        check(original);
        check(original);

        const auto first = provider.occurrences[0];
        provider.occurrences[0] = provider.occurrences[1];
        provider.occurrences[1] = first;

        const Count reordered[] = {1, 0, 2};
        check(reordered);
        check(reordered);
      },

      [&] { EXPECT(False); });
}
