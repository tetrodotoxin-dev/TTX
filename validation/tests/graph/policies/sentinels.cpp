// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/graph/policies/constant.hpp"
#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.hpp"
#include "ttx/graph/projection.hpp"
#include "validation/tests/graph/fixtures/value.hpp"

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
  Count reachability_queries = 0;

  auto get_abi() -> ttx_abstract {
    static const ttx_abstract_ops operations = {
      supports, bind, lookup, visit, get_reachability};

    return ttx_abstract(this, &operations);
  }

  static auto get_reachability(void* context) -> ttx_abstract_reachability {
    ++Data::cast<RouteProvider>(context)->reachability_queries;
    return TTX_ABSTRACT_REACHABLE;
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

// Reachability establishes a target even when its provider supplies none of
// the consumer's UUIDs. The operation makes one call through the published
// context and table, with zero support or bind probes.
VALIDATION_TEST(Sentinels, reachable_without_known_contracts) {
  RouteProvider provider{"next"_view, ttx_unknown()};
  const Abstract abstract(provider.get_abi());
  EXPECT(abstract.get_reachability() == Abstract::Reachability::Reachable);
  EXPECT_EQ(provider.reachability_queries, Count(1));
  EXPECT_EQ(provider.negotiations, Count(0));
  EXPECT_EQ(provider.resolutions, Count(0));

  EXPECT(abstract.supports<Abstract>() == Negotiation::Status::Unknown);
  EXPECT(abstract.supports<Policies::None>() == Negotiation::Status::Rejected);
  EXPECT(
      abstract.supports<Policies::Unknown>() == Negotiation::Status::Rejected);
  EXPECT(
      abstract.supports(Identity::Uuid(0, 2)) == Negotiation::Status::Unknown);
  EXPECT_EQ(provider.reachability_queries, Count(1));

  EXPECT(
      abstract.resolve_route("next"_view).get_reachability() ==
      Abstract::Reachability::Indeterminate);
}

// Support identifies each sentinel. A caller obtains its graph location by
// binding Abstract, so neither reserved policy request supplies a record.
VALIDATION_TEST(Sentinels, reserved_policies) {
  static_assert(TTX_NONE_ID_HIGH == 0 && TTX_NONE_ID_LOW == 0);
  static_assert(TTX_UNKNOWN_ID_HIGH == 0 && TTX_UNKNOWN_ID_LOW == 1);
  RouteProvider present{"next"_view, ttx_none()};
  const ttx_abstract values[] = {ttx_none(), ttx_unknown(), present.get_abi()};
  const perimortem_uuid ids[] = {
    Policies::None::contract_id, Policies::Unknown::contract_id};

  const ttx_negotiation_status support[][2] = {
    {TTX_NEGOTIATION_SATISFIED, TTX_NEGOTIATION_REJECTED},
    {TTX_NEGOTIATION_UNKNOWN, TTX_NEGOTIATION_SATISFIED},
    {TTX_NEGOTIATION_REJECTED, TTX_NEGOTIATION_REJECTED},
  };

  const ttx_negotiation_status binding[][2] = {
    {TTX_NEGOTIATION_REJECTED, TTX_NEGOTIATION_REJECTED},
    {TTX_NEGOTIATION_UNKNOWN, TTX_NEGOTIATION_UNKNOWN},
    {TTX_NEGOTIATION_REJECTED, TTX_NEGOTIATION_REJECTED},
  };

  const ttx_abstract_reachability reachability[] = {
    TTX_ABSTRACT_UNREACHABLE, TTX_ABSTRACT_INDETERMINATE,
    TTX_ABSTRACT_REACHABLE};

  for (Count row = 0; row < 3; ++row) {
    const auto api = values[row];
    EXPECT_EQ(api.operations->get_reachability(api.context), reachability[row]);
    for (Count request = 0; request < 2; ++request) {
      EXPECT_EQ(
          api.operations->supports(api.context, ids[request]),
          support[row][request]);

      ttx_abstract output{};
      const auto status =
          api.operations->bind(api.context, ids[request], &output);
      EXPECT_EQ(status, binding[row][request]);
    }
  }
}

// A search exposes the number of completed checks while its target is
// unsettled. An unsuccessful search establishes absence at this observation.
// A later installation can still make the original route select a target.
struct Search
    : Projection<Search, 0x2ca9c41248f74ee4ULL, 0x9e7112fc472593f1ULL> {
  U8 completed = 0;
  auto get_reachability() const -> ttx_abstract_reachability {
    return completed ? TTX_ABSTRACT_UNREACHABLE : TTX_ABSTRACT_INDETERMINATE;
  }

  auto supports_contract(Identity::Uuid contract) const
      -> ttx_negotiation_status {
    return contract == Validation::Value::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  auto bind_contract(Identity::Uuid contract, void* output)
      -> ttx_negotiation_status {
    const auto status = supports_contract(contract);
    if (status == TTX_NEGOTIATION_SATISFIED) {
      *Data::cast<value_api>(output) = {
        this,
        [](void* context) { return Data::cast<Search>(context)->completed; }};
    }

    return status;
  }
};

VALIDATION_TEST(Sentinels, reachability_does_not_identify_policy) {
  Search search;
  RouteProvider origin{"product"_view, ttx_unknown()};
  const Abstract root(origin.get_abi());
  const auto unknown = root.resolve_route("product"_view);
  origin.selected = search.get_abstract().get_abi();

  const auto pending = root.resolve_route("product"_view);
  EXPECT(unknown.get_reachability() == Abstract::Reachability::Indeterminate);
  EXPECT(pending.get_reachability() == unknown.get_reachability());
  EXPECT(
      pending.supports<Policies::Unknown>() == Negotiation::Status::Rejected);
  EXPECT_NOT(unknown.bind<Validation::Value>());

  const auto progress = pending.bind<Validation::Value>();
  ASSERT(progress);

  search.completed = 1;

  const auto missing = root.resolve_route("product"_view);
  EXPECT(missing.get_reachability() == Abstract::Reachability::Unreachable);
  EXPECT(missing.supports<Policies::None>() == Negotiation::Status::Rejected);
  EXPECT(
      missing.supports<Policies::Constant>() == Negotiation::Status::Rejected);
  progress.visit(
      [&](Validation::Value value) { EXPECT_EQ(value.read(), U8(1)); },
      [&] { EXPECT(False); });

  RouteProvider installed{"self"_view, origin.get_abi()};
  origin.selected = installed.get_abi();

  const auto product = root.resolve_route("product"_view);
  EXPECT(product.get_reachability() == Abstract::Reachability::Reachable);
  EXPECT(product == Abstract(installed.get_abi()));
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

      abstract.bind<Abstract>().visit(
          [&](Abstract bound) {
            EXPECT(bound.get_reachability() == terminal.get_reachability());
          },

          [&]() { EXPECT(False); });
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
  constexpr auto unrelated =
      Identity::Uuid(0x44f044cf9d8e4b8fULL, 0x81b4ce5aa42be54eULL);

  EXPECT(unknown.supports<Abstract>() == Negotiation::Status::Satisfied);
  EXPECT(
      unknown.supports<Policies::Unknown>() == Negotiation::Status::Satisfied);
  EXPECT(
      unknown.supports<Policies::Constant>() == Negotiation::Status::Unknown);
  EXPECT(none.supports<Abstract>() == Negotiation::Status::Satisfied);
  EXPECT(none.supports<Policies::Constant>() == Negotiation::Status::Satisfied);
  EXPECT(none.supports<Policies::Unknown>() == Negotiation::Status::Rejected);
  EXPECT(unknown.supports(unrelated) == Negotiation::Status::Unknown);
  EXPECT(none.supports(unrelated) == Negotiation::Status::Rejected);
}

// Binding Abstract preserves the sentinel's meaning at the returned location.
// Other binds convey permanent refusal on None and uncertainty on Unknown.
VALIDATION_TEST(Sentinels, policy_binding) {
  const auto none = ttx_none();
  ttx_abstract output{};
  ASSERT_EQ(
      none.operations->bind(none.context, Abstract::contract_id, &output),
      TTX_NEGOTIATION_SATISFIED);
  EXPECT(
      Abstract(output).get_reachability() ==
      Abstract::Reachability::Unreachable);

  const auto unknown = ttx_unknown();
  ASSERT_EQ(
      unknown.operations->bind(unknown.context, Abstract::contract_id, &output),
      TTX_NEGOTIATION_SATISFIED);
  EXPECT(
      Abstract(output).get_reachability() ==
      Abstract::Reachability::Indeterminate);
  EXPECT(
      Abstract(output).supports<Policies::Unknown>() ==
      Negotiation::Status::Satisfied);

  const perimortem_uuid policies[] = {
    Policies::None::contract_id, Policies::Constant::contract_id,
    Policies::Unknown::contract_id};

  for (const auto id : policies) {
    EXPECT_EQ(
        none.operations->bind(none.context, id, &output),
        TTX_NEGOTIATION_REJECTED);
    EXPECT_EQ(
        unknown.operations->bind(unknown.context, id, &output),
        TTX_NEGOTIATION_UNKNOWN);
  }

  constexpr auto unrelated =
      Identity::Uuid(0x44f044cf9d8e4b8fULL, 0x81b4ce5aa42be54eULL);
  EXPECT_EQ(
      none.operations->bind(none.context, unrelated, &output),
      TTX_NEGOTIATION_REJECTED);
  EXPECT_EQ(
      unknown.operations->bind(unknown.context, unrelated, &output),
      TTX_NEGOTIATION_UNKNOWN);
}

VALIDATION_TEST(Sentinels, navigation_and_unknown) {
  const auto unknown = Abstract(ttx_unknown());
  unknown.bind<Abstract>().visit(
      [&](Abstract root) {
        EXPECT(
            root.get_reachability() == Abstract::Reachability::Indeterminate);
        EXPECT(
            root.resolve_route("remaining"_view).get_reachability() ==
            Abstract::Reachability::Indeterminate);
      },

      [&] { EXPECT(False); });

  EXPECT_NOT(unknown.bind<Policies::Constant>());
  EXPECT_NOT(Abstract(ttx_none()).bind<Policies::Unknown>());
}
