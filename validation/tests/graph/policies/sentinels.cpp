// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/graph/policies/constant.hpp"
#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness Sentinels = {
  .name = "TTX::Sentinels",
};

// RouteProvider publishes one route to an independently supplied Abstract.
// Ordinary UUID requests stay Unknown, so the tests verify that traversal
// reaches the next provider using only the published Abstract operations.
struct RouteProvider {
  View::Bytes route;
  ttx_abstract selected;
  Count resolutions = 0;
  Count negotiations = 0;
  Count classifications = 0;

  auto get_abi() -> ttx_abstract {
    static const ttx_abstract_ops operations = {
      supports, bind, lookup, visit, state};

    return ttx_abstract(this, &operations);
  }

  static auto state(void* context) -> ttx_abstract_state {
    ++Data::cast<RouteProvider>(context)->classifications;
    return TTX_ABSTRACT_PRESENT;
  }

  static auto supports(void* context, perimortem_uuid contract)
      -> ttx_negotiation_status {
    if (contract.high == TTX_NONE_ID_HIGH &&
        (contract.low == TTX_NONE_ID_LOW ||
         contract.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    ++Data::cast<RouteProvider>(context)->negotiations;
    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid contract, void* output)
      -> ttx_negotiation_status {
    return supports(context, contract);
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    auto& provider = *Data::cast<RouteProvider>(context);
    ++provider.resolutions;

    return View::Bytes(route) == provider.route ? provider.selected
                                                : ttx_none();
  }

  static auto visit(void* context, ttx_graph_visitor callback) -> void {
    auto& provider = *Data::cast<RouteProvider>(context);
    callback.callback(
        callback.callback_context, provider.route, provider.selected);
  }
};

// Classification identifies an ordinary node even when it supplies none of
// the consumer's UUIDs. The operation makes one call through the published
// context and table, with zero support or bind probes.
VALIDATION_TEST(Sentinels, present_without_known_contracts) {
  RouteProvider provider{"next"_view, ttx_unknown()};
  const Abstract abstract(provider.get_abi());
  EXPECT(abstract.get_state() == Abstract::State::Present);
  EXPECT_EQ(provider.classifications, Count(1));
  EXPECT_EQ(provider.negotiations, Count(0));
  EXPECT_EQ(provider.resolutions, Count(0));

  EXPECT(abstract.supports<Abstract>() == Negotiation::Status::Unknown);
  EXPECT(abstract.supports<Policies::None>() == Negotiation::Status::Rejected);
  EXPECT(
      abstract.supports<Policies::Unknown>() == Negotiation::Status::Rejected);
  EXPECT(
      abstract.supports(Identity::Uuid(0, 2)) == Negotiation::Status::Unknown);
  EXPECT_EQ(provider.classifications, Count(1));

  EXPECT(
      abstract.resolve_route("next"_view).get_state() ==
      Abstract::State::Unknown);
}

// Reserved UUID requests follow the classified graph state through both
// support queries and bindings. Ordinary contract requests remain independent.
VALIDATION_TEST(Sentinels, reserved_states) {
  static_assert(TTX_NONE_ID_HIGH == 0 && TTX_NONE_ID_LOW == 0);
  static_assert(TTX_UNKNOWN_ID_HIGH == 0 && TTX_UNKNOWN_ID_LOW == 1);
  RouteProvider present{"next"_view, ttx_none()};
  const ttx_abstract values[] = {ttx_none(), ttx_unknown(), present.get_abi()};
  const perimortem_uuid ids[] = {
    Policies::None::contract_id, Policies::Unknown::contract_id};

  const ttx_negotiation_status expected[][2] = {
    {TTX_NEGOTIATION_SATISFIED, TTX_NEGOTIATION_REJECTED},
    {TTX_NEGOTIATION_REJECTED, TTX_NEGOTIATION_SATISFIED},
    {TTX_NEGOTIATION_REJECTED, TTX_NEGOTIATION_REJECTED},
  };

  for (Count state = 0; state < 3; ++state) {
    const auto api = values[state];
    EXPECT_EQ(
        api.operations->get_state(api.context), ttx_abstract_state(state));
    for (Count request = 0; request < 2; ++request) {
      EXPECT_EQ(
          api.operations->supports(api.context, ids[request]),
          expected[state][request]);

      ttx_abstract output{};
      const auto status =
          api.operations->bind(api.context, ids[request], &output);
      EXPECT_EQ(status, expected[state][request]);
      if (status == TTX_NEGOTIATION_SATISFIED) {
        EXPECT_EQ(
            output.operations->get_state(output.context),
            ttx_abstract_state(state));
      }
    }
  }
}

// None and Unknown must return themselves for every route. Empty, binary and
// named routes exercise continued traversal after a terminal is reached.
// Visitation completes with zero callbacks.
VALIDATION_TEST(Sentinels, terminal_routes) {
  const Abstract terminals[] = {Abstract(ttx_none()), Abstract(ttx_unknown())};
  const U8 binary_route[] = {7, 0, 9};

  for (const auto terminal : terminals) {
    EXPECT(
        terminal.resolve_route(View::Bytes())
            .resolve_route(View::Bytes(binary_route))
            .resolve_route("remaining"_view) == terminal);

    Count visits = 0;
    auto callback = [&](View::Bytes route, Abstract abstract) { ++visits; };
    terminal.visit_routes(Abstract::Visitor(callback));
    EXPECT_EQ(visits, Count(0));
  }
}

// The same chain reaches its terminal result when any one of its three routes
// selects it. Only the final result is bound, so intermediate providers need
// no knowledge of the UUID requested by the consumer.
VALIDATION_TEST(Sentinels, terminal_entry_position) {
  const Abstract terminals[] = {Abstract(ttx_none()), Abstract(ttx_unknown())};

  for (const auto terminal : terminals) {
    for (Count position = 0; position < 3; ++position) {
      RouteProvider third{"c"_view, terminal.get_abi()};
      RouteProvider second{"b"_view, third.get_abi()};
      RouteProvider first{"a"_view, second.get_abi()};

      if (position == 0) {
        first.selected = terminal.get_abi();
      } else if (position == 1) {
        second.selected = terminal.get_abi();
      }

      const Abstract root(first.get_abi());
      const auto abstract =
          root.resolve_route("a"_view).resolve_route("b"_view).resolve_route(
              "c"_view);
      EXPECT(abstract == terminal);
      EXPECT_EQ(
          first.resolutions + second.resolutions + third.resolutions,
          position + 1);
      EXPECT_EQ(
          first.negotiations + second.negotiations + third.negotiations,
          Count(0));

      Bool bound = False;
      if (terminal == Abstract(ttx_none())) {
        abstract.bind<Policies::None>().visit(
            [&](Policies::None policy) {
              bound = True;
              EXPECT(Abstract(policy.get_abi()) == terminal);
            },

            [&]() { EXPECT(False); });
      } else {
        abstract.bind<Policies::Unknown>().visit(
            [&](Policies::Unknown policy) {
              bound = True;
              EXPECT(Abstract(policy.get_abi()) == terminal);
            },

            [&]() { EXPECT(False); });
      }

      EXPECT(bound);
    }
  }
}

// A fresh request observes the original provider's new selection. The earlier
// Unknown remains terminal even after the original route becomes available.
VALIDATION_TEST(Sentinels, retry_original_route) {
  RouteProvider available{"child"_view, ttx_none()};
  RouteProvider provider{"selected"_view, ttx_unknown()};
  const Abstract root(provider.get_abi());
  const auto first = root.resolve_route("selected"_view);
  EXPECT(first == Abstract(ttx_unknown()));

  provider.selected = available.get_abi();

  const auto second = root.resolve_route("selected"_view);
  EXPECT(second == Abstract(available.get_abi()));
  EXPECT(second.resolve_route("child"_view) == Abstract(ttx_none()));
  EXPECT(first.resolve_route("child"_view) == first);
  EXPECT_EQ(provider.resolutions, Count(2));
  EXPECT_EQ(provider.negotiations, Count(0));
  EXPECT_EQ(available.negotiations, Count(0));
}

// Support queries report policy status without binding an API. Unknown
// establishes its own contract while leaving Constant undetermined.
VALIDATION_TEST(Sentinels, support_properties) {
  const auto unknown = Policies::Unknown::get_unknown().get_abstract();
  const auto none = Policies::None::get_none().get_abstract();

  EXPECT(unknown.supports<Abstract>() == Negotiation::Status::Satisfied);
  EXPECT(
      unknown.supports<Policies::Unknown>() == Negotiation::Status::Satisfied);
  EXPECT(
      unknown.supports<Policies::Constant>() == Negotiation::Status::Unknown);
  EXPECT(none.supports<Policies::Constant>() == Negotiation::Status::Unknown);
  EXPECT(none.supports<Policies::Unknown>() == Negotiation::Status::Rejected);
}

// Sentinel UUIDs return complete Abstract records through the established
// Graph ABI. Refused and undetermined bindings produce an empty Option.
VALIDATION_TEST(Sentinels, policy_binding) {
  const auto none = ttx_none();
  ttx_abstract output{};
  EXPECT_EQ(
      none.operations->bind(none.context, Policies::None::contract_id, &output),
      TTX_NEGOTIATION_SATISFIED);
  EXPECT(Abstract(output).get_state() == Abstract::State::None);

  const auto unknown = ttx_unknown();
  EXPECT_EQ(
      unknown.operations->bind(
          unknown.context, Policies::Unknown::contract_id, &output),
      TTX_NEGOTIATION_SATISFIED);
  EXPECT(Abstract(output).get_state() == Abstract::State::Unknown);
  EXPECT(Abstract(none).bind<Policies::None>());
  EXPECT_NOT(Abstract(none).bind<Policies::Constant>());
  EXPECT_NOT(Abstract(none).bind<Policies::Unknown>());
}

VALIDATION_TEST(Sentinels, navigation_and_unknown) {
  const auto unknown = Abstract(ttx_unknown());
  unknown.bind<Abstract>().visit(
      [&](Abstract root) {
        EXPECT(root.get_state() == Abstract::State::Unknown);
        EXPECT(
            root.resolve_route("remaining"_view).get_state() ==
            Abstract::State::Unknown);
      },

      [&] { EXPECT(False); });

  EXPECT_NOT(unknown.bind<Policies::Constant>());
  EXPECT_NOT(Abstract(ttx_none()).bind<Policies::Unknown>());
}
