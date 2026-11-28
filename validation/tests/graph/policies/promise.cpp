// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/promise.h"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/graph/policies/alias.h"
#include "ttx/graph/policies/none.h"
#include "ttx/graph/policies/unknown.h"
#include "ttx/graph/providers/alias.hpp"
#include "ttx/graph/providers/promise.hpp"
#include "validation/tests/graph/fixtures/observation.h"
#include "validation/tests/graph/fixtures/value.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;

static Harness Promises = {.name = "TTX::Promise"};

VALIDATION_TEST(Promises, previously_issued_handle_forwards_all_operations) {
  Providers::Promise p, q;
  const auto held = p.get_abstract();
  const auto other = q.get_abstract();
  const Abstract unknown = ttx_unknown();
  ASSERT(held.supports<Policies::Promise>() == Negotiation::Status::Satisfied);
  EXPECT(held.supports<Policies::Unknown>() == Negotiation::Status::Unknown);
  EXPECT(held.resolve() == unknown);
  EXPECT(held.resolve_route({}) == unknown);
  EXPECT(held.get_reachability() == Abstract::Reachability::Indeterminate);

  const ttx_abstract raw = held;
  ttx_abstract output{};
  EXPECT(
      raw.operations->bind(
          raw.context, Policies::Promise::contract_id, &output) ==
      Negotiation::Status::Unknown);
  EXPECT_NOT(held.bind<Abstract>());

  Count visits = 0;
  auto visit = [&](View::Bytes, Abstract) { ++visits; };
  held.visit_routes(Abstract::Visitor(visit));
  EXPECT_EQ(visits, Count(0));

  EXPECT(p.fulfill(unknown) == Negotiation::Status::Unknown);
  EXPECT(
      p.fulfill(Abstract({nullptr, nullptr})) == Negotiation::Status::Rejected);
  ASSERT(p.fulfill(other) == Negotiation::Status::Satisfied);
  EXPECT(p.get_abstract() == other);
  EXPECT(held.supports<Policies::Promise>() == Negotiation::Status::Satisfied);

  observation_subject provider{};
  provider.answer = TTX_NEGOTIATION_SATISFIED;

  const Abstract target = observation_abstract(&provider);
  ASSERT(q.fulfill(target) == Negotiation::Status::Satisfied);
  EXPECT(q.fulfill(target) == Negotiation::Status::Satisfied);
  EXPECT(p.fulfill(target) == Negotiation::Status::Rejected);
  EXPECT(p.get_abstract() == target);
  EXPECT(
      held.supports<Policies::Promise>() ==
      target.supports<Policies::Promise>());
  EXPECT(held.get_reachability() == target.get_reachability());
  EXPECT(held.resolve() == target);
  EXPECT(
      held.resolve_route("missing"_view) ==
      target.resolve_route("missing"_view));
  held.bind<Abstract>().visit(
      [&](Abstract view) {
        EXPECT(
            view.supports<Validation::Value>() ==
            Negotiation::Status::Satisfied);
      },

      [&] { EXPECT(False); });
  held.bind<Validation::Value>().visit(
      [&](Validation::Value v) { EXPECT_EQ(v.read(), U8(1)); },
      [&] { EXPECT(False); });
  held.visit_routes(Abstract::Visitor(visit));
  EXPECT_EQ(visits, Count(2));
}

VALIDATION_TEST(Promises, circular_obligations_and_alias_targets) {
  Providers::Promise p, q;
  const auto a = p.get_abstract();
  const auto b = q.get_abstract();
  ASSERT(p.fulfill(b) == Negotiation::Status::Satisfied);
  ASSERT(q.fulfill(a) == Negotiation::Status::Satisfied);
  EXPECT(a.supports<Policies::Promise>() == Negotiation::Status::Satisfied);
  EXPECT(a.resolve() == Abstract(ttx_unknown()));
  EXPECT_NOT(b.bind<Abstract>());
  EXPECT(b.resolve_route({}) == Abstract(ttx_unknown()));

  Providers::Promise promise;
  const auto held = promise.get_abstract();
  Providers::Alias alias(held);
  ASSERT(promise.fulfill(alias) == Negotiation::Status::Satisfied);
  EXPECT(held.supports<Policies::Alias>() == Negotiation::Status::Satisfied);
  EXPECT(held.resolve() == Abstract(ttx_unknown()));
  ASSERT(alias.set_referent(ttx_none()) == Negotiation::Status::Satisfied);
  EXPECT(held.resolve() == Abstract(ttx_none()));
  EXPECT(held.get_reachability() == Abstract::Reachability::Reachable);
}

VALIDATION_TEST(Promises, fulfillment_during_foreign_observation) {
  Providers::Promise promise;
  const auto held = promise.get_abstract();
  const ttx_abstract_ops operations = {
    [](void* context, perimortem_uuid) {
      auto& owner = *Data::cast<Providers::Promise>(context);
      owner.fulfill(ttx_none());
      return Negotiation::Status::Unknown;
    },

    [](void*, perimortem_uuid, void*) { return Negotiation::Status::Unknown; },
    [](void*, perimortem_view_bytes) { return ttx_unknown(); },
    [](void*, ttx_graph_visitor) {},
    [](void*) { return TTX_ABSTRACT_REACHABLE; },
    ttx_resolve_self};

  const Abstract candidate(&promise, operations);
  EXPECT(promise.fulfill(candidate) == Negotiation::Status::Rejected);
  EXPECT(held.resolve() == Abstract(ttx_none()));
}

// An independent dependency forwards back through the original handle.
// Repeating that hop before a response arrives leaves the obligation pending.
struct ForeignDependency {
  Abstract original;
  Count calls = 0;

  auto get_abstract() -> Abstract {
    static const ttx_abstract_ops operations = {
      [](void* context, perimortem_uuid id) {
        auto& self = *Data::cast<ForeignDependency>(context);
        if (Identity::Uuid(id) == Policies::Unknown::contract_id) {
          return Negotiation::Status::Unknown;
        }

        ++self.calls;
        return self.original.supports(id);
      },

      [](void* context, perimortem_uuid id, void* output) {
        const ttx_abstract original =
            Data::cast<ForeignDependency>(context)->original;
        return original.operations->bind(original.context, id, output);
      },

      [](void* context, perimortem_view_bytes name) -> ttx_abstract {
        return Data::cast<ForeignDependency>(context)->original.resolve_route(
            View::Bytes(name));
      },

      [](void* context, ttx_graph_visitor visitor) {
        const ttx_abstract original =
            Data::cast<ForeignDependency>(context)->original;
        original.operations->visit_routes(original.context, visitor);
      },

      [](void* context) {
        const ttx_abstract original =
            Data::cast<ForeignDependency>(context)->original;
        return original.operations->get_reachability(original.context);
      },

      [](ttx_abstract self) -> ttx_abstract {
        return Data::cast<ForeignDependency>(self.context)->original.resolve();
      }};

    return Abstract(this, operations);
  }
};

VALIDATION_TEST(Promises, foreign_dependency_returns_to_same_request) {
  Providers::Promise promise;
  const auto held = promise.get_abstract();
  ForeignDependency foreign{held};
  ASSERT(
      promise.fulfill(foreign.get_abstract()) ==
      Negotiation::Status::Satisfied);
  EXPECT(held.supports<Policies::Promise>() == Negotiation::Status::Satisfied);
  EXPECT_EQ(foreign.calls, Count(1));
  EXPECT_NOT(held.bind<Abstract>());
  EXPECT(held.get_reachability() == Abstract::Reachability::Indeterminate);
  EXPECT(held.resolve_route("x"_view) == Abstract(ttx_unknown()));
  EXPECT(held.resolve() == Abstract(ttx_unknown()));

  Count visits = 0;
  auto count = [&](View::Bytes, Abstract) { ++visits; };
  held.visit_routes(Abstract::Visitor(count));
  EXPECT_EQ(visits, Count(0));
}

VALIDATION_TEST(Promises, delivered_callback_can_visit_the_same_promise) {
  Providers::Promise promise;
  const auto held = promise.get_abstract();
  observation_subject provider{};
  provider.answer = TTX_NEGOTIATION_SATISFIED;
  ASSERT(
      promise.fulfill(observation_abstract(&provider)) ==
      Negotiation::Status::Satisfied);

  Count outer = 0, nested = 0;
  auto again = [&](View::Bytes, Abstract) { ++nested; };
  auto first = [&](View::Bytes, Abstract) {
    ++outer;
    if (outer == 1) {
      held.visit_routes(Abstract::Visitor(again));
    }
  };

  held.visit_routes(Abstract::Visitor(first));
  EXPECT_EQ(outer, Count(2));
  EXPECT_EQ(nested, Count(2));
}
