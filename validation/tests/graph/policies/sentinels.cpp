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
// Its UUID requests stay Unknown, allowing the tests to verify that traversal
// reaches the next provider using only the published Abstract operations.
struct RouteProvider {
  View::Bytes route;
  ttx_abstract selected;
  Count resolutions = 0;
  Count negotiations = 0;

  auto get_abi() -> ttx_abstract {
    static const ttx_abstract_ops operations = {supports, bind,   data,
                                                resolve,  lookup, visit};

    return ttx_abstract(this, &operations);
  }

  static auto supports(void* context, perimortem_uuid contract)
      -> ttx_negotiation_status {
    ++Data::cast<RouteProvider>(context)->negotiations;
    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(
      void* context,
      perimortem_uuid contract,
      ttx_storage requested) -> ttx_negotiation_status {
    ++Data::cast<RouteProvider>(context)->negotiations;
    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto data(void* context) -> perimortem_view_bytes { return {}; }

  static auto resolve(void* context) -> ttx_abstract {
    return Data::cast<RouteProvider>(context)->get_abi();
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
    callback.callback(callback.context, provider.route, provider.selected);
  }
};

// None and Unknown must return themselves for every route. Empty, binary and
// named routes exercise continued traversal after a terminal is reached.
// Visitation completes with zero callbacks.
VALIDATION_TEST(Sentinels, terminal_routes) {
  const Abstract terminals[] = {Abstract(ttx_none()), Abstract(ttx_unknown())};
  const U8 binary_route[] = {7, 0, 9};

  for (const auto terminal : terminals) {
    EXPECT(terminal.resolve() == terminal);
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

            [&](Negotiation::Status status) { EXPECT(False); });
      } else {
        abstract.bind<Policies::Unknown>().visit(
            [&](Policies::Unknown policy) {
              bound = True;
              EXPECT(Abstract(policy.get_abi()) == terminal);
            },

            [&](Negotiation::Status status) { EXPECT(False); });
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
  EXPECT(none.supports<Policies::Unknown>() == Negotiation::Status::Unknown);
}

// Keep the outer record and every table slot the same size. Only get_data's
// return description differs. Binding rejects that form because the canonical
// description includes the operation table's function signatures.
struct WrongAbstractOperations {
  decltype(ttx_abstract_ops::supports) supports;
  decltype(ttx_abstract_ops::bind) bind;
  U64 (*get_data)(void*);
  decltype(ttx_abstract_ops::resolve) resolve;
  decltype(ttx_abstract_ops::resolve_route) resolve_route;
  decltype(ttx_abstract_ops::visit_routes) visit_routes;
};

struct WrongAbstract {
  void* context;
  const WrongAbstractOperations* operations;
};

TTX_DATA_RECORD(
    WrongAbstractOperations,
    TTX_DATA_MEMBER(WrongAbstractOperations, supports),
    TTX_DATA_MEMBER(WrongAbstractOperations, bind),
    TTX_DATA_MEMBER(WrongAbstractOperations, get_data),
    TTX_DATA_MEMBER(WrongAbstractOperations, resolve),
    TTX_DATA_MEMBER(WrongAbstractOperations, resolve_route),
    TTX_DATA_MEMBER(WrongAbstractOperations, visit_routes));
TTX_DATA_RECORD(
    WrongAbstract,
    TTX_DATA_MEMBER(WrongAbstract, context),
    TTX_DATA_MEMBER(WrongAbstract, operations));

VALIDATION_TEST(Sentinels, nested_table_abi) {
  static_assert(sizeof(WrongAbstract) == sizeof(ttx_abstract));
  static_assert(sizeof(WrongAbstractOperations) == sizeof(ttx_abstract_ops));
  const auto& form = Form::Compiled<
      Form::Native<WrongAbstract>::reference>::get_representation();
  WrongAbstract output = WrongAbstract();
  const Form::Storage target(
      ttx_storage(&form.get_abi(), Data::cast<U8>(&output), sizeof(output)));
  const auto query = Policies::None::get_none().get_abstract().get_query();
  EXPECT(
      query.bind(Abstract::contract_id, target) ==
      Negotiation::Status::Rejected);
  EXPECT(output.context == nullptr && output.operations == nullptr);
  query.bind<Abstract>().visit(
      [&](Abstract accepted) {
        EXPECT(accepted == Policies::None::get_none().get_abstract());
      },

      [&](Negotiation::Status) { EXPECT(False); });
}

// Binding None or Unknown returns their governing Abstract operations. A
// different requested representation is rejected before the output can be used.
VALIDATION_TEST(Sentinels, policy_binding) {
  const auto none = ttx_none();
  ttx_abstract answer = ttx_abstract();
  const auto& form = Form::Compiled<
      Form::Native<Policies::None::Api>::reference>::get_representation();
  const ttx_storage output =
      ttx_storage(&form.get_abi(), Data::cast<U8>(&answer), sizeof(answer));
  EXPECT_EQ(
      none.operations->bind(none.context, Policies::None::contract_id, output),
      TTX_NEGOTIATION_SATISFIED);

  U32 untouched = 42;
  const auto& operational =
      Form::Compiled<Form::Native<U32>::reference>::get_representation();
  const ttx_storage wrong = ttx_storage(
      &operational.get_abi(), Data::cast<U8>(&untouched), sizeof(untouched));
  EXPECT_EQ(
      none.operations->bind(none.context, Policies::None::contract_id, wrong),
      TTX_NEGOTIATION_REJECTED);
  EXPECT_EQ(untouched, U32(42));

  const auto unknown = ttx_unknown();
  EXPECT_EQ(
      unknown.operations->bind(
          unknown.context, Policies::Unknown::contract_id, output),
      TTX_NEGOTIATION_SATISFIED);
  Abstract(none).bind<Policies::None>().visit(
      [&](Policies::None) {}, [&](Negotiation::Status) { EXPECT(False); });
  Abstract(none).bind<Policies::Constant>().visit(
      [&](Policies::Constant) { EXPECT(False); },
      [&](Negotiation::Status failure) {
        EXPECT(failure == Negotiation::Status::Unknown);
      });
}

// The Unknown sentinel binds its own Abstract with the same context and byte
// observation. Further negotiation of Constant stays Unknown, preserving the
// sentinel's policy through that bound API.
VALIDATION_TEST(Sentinels, navigation_and_unknown) {
  const auto unknown = Abstract(ttx_unknown());
  unknown.get_query().bind<Abstract>().visit(
      [&](Abstract root) {
        EXPECT(root.get_abi().context == unknown.get_abi().context);
        EXPECT(root.get_data() == "Unknown"_view);
      },

      [&](Negotiation::Status) { EXPECT(False); });

  unknown.bind<Policies::Constant>().visit(
      [&](Policies::Constant) { EXPECT(False); },
      [&](Negotiation::Status failure) {
        EXPECT(failure == Negotiation::Status::Unknown);
      });

  Policies::None::get_none().get_abstract().bind<Policies::Unknown>().visit(
      [&](Policies::Unknown) { EXPECT(False); },
      [&](Negotiation::Status failure) {
        EXPECT(failure == Negotiation::Status::Unknown);
      });
}

// Each tested Graph interface checks its required Abstract operations. A
// provider reporting Satisfied with an empty record is rejected by those
// checks.
VALIDATION_TEST(Sentinels, empty_table_admission) {
  Negotiation::Query query(ttx_semantic_query(
      nullptr,
      [](void*, perimortem_uuid, ttx_storage output) -> ttx_negotiation_status {
        (void)output;
        return TTX_NEGOTIATION_SATISFIED;
      },

      [](void*, perimortem_uuid) -> ttx_negotiation_status {
        return TTX_NEGOTIATION_SATISFIED;
      }));

  query.bind<Policies::Constant>().visit(
      [&](Policies::Constant) { EXPECT(False); },
      [&](Negotiation::Status failure) {
        EXPECT(failure == Negotiation::Status::Rejected);
      });

  query.bind<Abstract>().visit(
      [&](Abstract) { EXPECT(False); },
      [&](Negotiation::Status failure) {
        EXPECT(failure == Negotiation::Status::Rejected);
      });
}
