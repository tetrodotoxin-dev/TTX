// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/alias.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/data/transfer.hpp"
#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.hpp"
#include "validation/tests/graph/fixtures/observation.h"
#include "validation/tests/graph/fixtures/value.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Data;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;

static Harness Aliases = {.name = "TTX::Alias"};

VALIDATION_TEST(Aliases, location_and_referent) {
  ttx_alias_slot storage;
  const Abstract location = Policies::Alias::initialize(storage);
  ttx_alias bound{};
  const auto slot = location.get_abi();
  ASSERT_EQ(
      slot.operations->bind(slot.context, Policies::Alias::contract_id, &bound),
      TTX_NEGOTIATION_SATISFIED);

  const Policies::Alias alias(bound);
  EXPECT(location.get_reachability() == Abstract::Reachability::Reachable);
  EXPECT(
      location.supports<Policies::Alias>() == Negotiation::Status::Satisfied);
  EXPECT(location.supports<Abstract>() == Negotiation::Status::Satisfied);
  EXPECT(
      location.supports<Policies::Unknown>() == Negotiation::Status::Rejected);
  EXPECT(location.supports<Policies::None>() == Negotiation::Status::Rejected);
  location.bind<Abstract>().visit(
      [&](Abstract rebound) { EXPECT(rebound == location); },
      [&] { EXPECT(False); });
  location.bind<Policies::Alias>().visit(
      [&](Policies::Alias other) {
        EXPECT(other.get_referent() == alias.get_referent());
      },

      [&] { EXPECT(False); });

  const Abstract initial = alias.get_referent();
  EXPECT(
      initial.supports<Policies::Unknown>() == Negotiation::Status::Satisfied);
  EXPECT(initial.supports<Policies::Alias>() == Negotiation::Status::Unknown);

  const Abstract none(ttx_none());
  ASSERT(alias.set_referent(none) == Negotiation::Status::Satisfied);
  EXPECT(alias.get_referent() == none);
  EXPECT(initial.get_reachability() == Abstract::Reachability::Indeterminate);
  EXPECT(location.get_reachability() == Abstract::Reachability::Reachable);
  EXPECT(location.supports<Policies::None>() == Negotiation::Status::Rejected);

  ttx_abstract output{};
  EXPECT_EQ(
      slot.operations->bind(slot.context, Policies::None::contract_id, &output),
      TTX_NEGOTIATION_REJECTED);

  // A reference can denote another reference, including itself. Each read
  // returns the stored location and lets the caller choose the next bind.
  ttx_alias_slot second_storage;
  const Abstract second = Policies::Alias::initialize(second_storage);
  ASSERT(alias.set_referent(second) == Negotiation::Status::Satisfied);
  ASSERT(alias.get_referent() == second);
  second.bind<Policies::Alias>().visit(
      [&](Policies::Alias inner) {
        EXPECT(inner.get_referent() == initial);
        EXPECT(inner.set_referent(none) == Negotiation::Status::Satisfied);
        EXPECT(alias.get_referent() == second);
        EXPECT(inner.get_referent() == none);
      },

      [&] { EXPECT(False); });
  ASSERT(alias.set_referent(location) == Negotiation::Status::Satisfied);
  EXPECT(alias.get_referent() == location);
  ASSERT(alias.set_referent(initial) == Negotiation::Status::Satisfied);
  EXPECT(alias.get_referent() == initial);
}

// The first callback redirects the Alias from A to B. After the visit the
// consumer binds Alias again on the original location and keeps both target
// Abstracts. It resolves the same route and reads a Value through each one.
VALIDATION_TEST(Aliases, redirect_during_observation) {
  observation_subject first{};
  observation_subject second{};
  first.value = 7;
  second.value = 23;
  first.answer = TTX_NEGOTIATION_SATISFIED;
  second.answer = TTX_NEGOTIATION_SATISFIED;

  const Abstract a(observation_abstract(&first));
  const Abstract b(observation_abstract(&second));
  ttx_alias_slot storage;
  const Abstract location = Policies::Alias::initialize(storage);
  ttx_alias bound{};
  const auto slot = location.get_abi();
  ASSERT_EQ(
      slot.operations->bind(slot.context, Policies::Alias::contract_id, &bound),
      TTX_NEGOTIATION_SATISFIED);

  const Policies::Alias alias(bound);
  ASSERT(alias.set_referent(a) == Negotiation::Status::Satisfied);

  ttx_alias copy{};
  auto source = alias.get_abi();
  ASSERT(
      Transfer::copy(Form::Storage::from(source), Form::Storage::from(copy)) ==
      Status::Success);

  const Policies::Alias other(copy);
  const Abstract captured = alias.get_referent();
  auto value = captured.bind<Validation::Value>();
  ASSERT(value);

  Count calls = 0;
  auto visit = [&](View::Bytes route, Abstract child) {
    EXPECT(child == a);
    EXPECT(captured.resolve_route(route) == a);
    ++calls;
    if (calls == 1) {
      EXPECT(other.set_referent(b) == Negotiation::Status::Satisfied);
    }
  };

  captured.visit_routes(Abstract::Visitor(visit));
  EXPECT_EQ(calls, Count(2));
  value.visit(
      [&](Validation::Value held) { EXPECT_EQ(held.read(), U8(8)); },
      [&] { EXPECT(False); });
  EXPECT(alias.get_referent() == b);
  location.bind<Policies::Alias>().visit(
      [&](Policies::Alias reread) {
        const Abstract current = reread.get_referent();
        const U8 route[] = {7, 0, 9};
        const auto original_child = captured.resolve_route(View::Bytes(route));
        const auto current_child = current.resolve_route(View::Bytes(route));
        original_child.bind<Validation::Value>().visit(
            [&](Validation::Value original) {
              EXPECT_EQ(original.read(), U8(9));
            },

            [&] { EXPECT(False); });
        current_child.bind<Validation::Value>().visit(
            [&](Validation::Value redirected) {
              EXPECT_EQ(redirected.read(), U8(24));
            },

            [&] { EXPECT(False); });
      },

      [&] { EXPECT(False); });
  EXPECT_NOT(location.bind<Validation::Value>());
  EXPECT(
      location.resolve_route("child"_view).supports<Policies::None>() ==
      Negotiation::Status::Satisfied);

  Count slot_visits = 0;
  auto count = [&](View::Bytes, Abstract) { ++slot_visits; };
  location.visit_routes(Abstract::Visitor(count));
  EXPECT_EQ(slot_visits, Count(0));
}

VALIDATION_TEST(Aliases, refused_replacement) {
  ttx_alias_slot storage;
  const auto slot = Policies::Alias::initialize(storage).get_abi();
  ttx_alias api{};
  ASSERT_EQ(
      slot.operations->bind(slot.context, Policies::Alias::contract_id, &api),
      TTX_NEGOTIATION_SATISFIED);

  const Policies::Alias alias(api);
  const Abstract retained(ttx_none());
  ASSERT(alias.set_referent(retained) == Negotiation::Status::Satisfied);
  EXPECT_EQ(
      api.operations->set_referent(api.context, {}), TTX_NEGOTIATION_REJECTED);
  EXPECT(alias.get_referent() == retained);

  const Abstract unknown(ttx_unknown());
  ttx_abstract_ops incomplete = *unknown.get_abi().operations;
  incomplete.get_reachability = nullptr;
  EXPECT_EQ(
      api.operations->set_referent(api.context, {&storage, &incomplete}),
      TTX_NEGOTIATION_REJECTED);
  EXPECT(alias.get_referent() == retained);

  // An independent provider can grant inspection while deciding each write
  // separately. A read through this API always supplies its original target.
  ttx_negotiation_status decision = TTX_NEGOTIATION_UNKNOWN;
  static const ttx_alias_ops operations = {
    [](void*) { return ttx_none(); },
    [](void* context, ttx_abstract) {
      return *Data::cast<ttx_negotiation_status>(context);
    }};

  const Policies::Alias restricted({&decision, &operations});
  const Abstract previous = restricted.get_referent();
  ASSERT(previous.supports<Policies::None>() == Negotiation::Status::Satisfied);
  EXPECT(restricted.set_referent(unknown) == Negotiation::Status::Unknown);
  EXPECT(restricted.get_referent() == previous);
  decision = TTX_NEGOTIATION_REJECTED;
  EXPECT(restricted.set_referent(unknown) == Negotiation::Status::Rejected);
  EXPECT(restricted.get_referent() == previous);
}
