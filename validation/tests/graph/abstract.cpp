// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/data/transfer.hpp"
#include "ttx/graph/policies/constant.hpp"
#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/ordered.hpp"
#include "ttx/graph/policies/unknown.hpp"
#include "validation/tests/graph/fixtures/observation.h"
#include "validation/tests/graph/fixtures/value.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Serialization;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness Abstracts = {
  .name = "TTX::Abstract",
};

VALIDATION_TEST(Abstracts, c_observations) {
  observation_subject provider{};
  const Abstract subject(observation_abstract(&provider));
  subject.bind<Abstract>().visit(
      [&](Abstract acquired) { EXPECT(acquired == subject); },
      [&]() { EXPECT(False); });
  subject.bind<Policies::Constant>().visit(
      [](Policies::Constant) {}, [&]() { EXPECT(False); });

  // Constant keeps the graph selection stable while successive typed reads
  // produce independent values from changing provider state.
  const U8 first = subject.bind<Validation::Value>().visit(
      [](Validation::Value value) { return value.read(); },
      [&] {
        EXPECT(False);
        return U8(0);
      });

  const U8 second = subject.bind<Validation::Value>().visit(
      [](Validation::Value value) { return value.read(); },
      [&] {
        EXPECT(False);
        return U8(0);
      });

  EXPECT_EQ(first, U8(1));
  EXPECT_EQ(second, U8(2));

  Count visited = 0;
  auto callback = [&](View::Bytes route, Abstract value) {
    ++visited;
    EXPECT_EQ(route.get_size(), Count(3));
    EXPECT_EQ(route.get_data()[1], U8(0));
    EXPECT(subject.resolve_route(route) == value);
  };

  subject.visit_routes(Abstract::Visitor(callback));
  EXPECT_EQ(visited, Count(1));
  EXPECT(
      subject.resolve_route("missing"_view) ==
      Policies::None::get_none().get_abstract());
}

// Copying an Abstract must preserve its published context and operations.
// Provider state may still change through those operations.
VALIDATION_TEST(Abstracts, copied_view) {
  observation_subject provider{};
  const Abstract subject(observation_abstract(&provider));
  const auto copy = subject;
  EXPECT(copy == subject);
  EXPECT(copy.get_abi().context == subject.get_abi().context);
  EXPECT(copy.get_abi().operations == subject.get_abi().operations);
  EXPECT_EQ(
      subject.bind<Validation::Value>().visit(
          [](Validation::Value value) { return value.read(); },
          [&] {
            EXPECT(False);
            return U8(0);
          }),

      U8(1));
  EXPECT_EQ(
      copy.bind<Validation::Value>().visit(
          [](Validation::Value value) { return value.read(); },
          [&] {
            EXPECT(False);
            return U8(0);
          }),

      U8(2));
  EXPECT_EQ(
      subject.bind<Validation::Value>().visit(
          [](Validation::Value value) { return value.read(); },
          [&] {
            EXPECT(False);
            return U8(0);
          }),

      U8(3));

  static_assert(sizeof(Abstract) == sizeof(ttx_abstract));
}

VALIDATION_TEST(Abstracts, fresh_abstract_binding) {
  observation_subject provider{};
  const Abstract subject(observation_abstract(&provider));
  const auto check = [&](Abstract expected) {
    subject.bind<Abstract>().visit(
        [&](Abstract answer) { EXPECT(answer == expected); },
        [&]() { EXPECT(False); });
  };

  provider.selected = ttx_none();
  check(Abstract(ttx_none()));

  provider.selected = ttx_unknown();
  check(Abstract(ttx_unknown()));

  const ttx_negotiation_status failures[] = {
    TTX_NEGOTIATION_UNKNOWN, TTX_NEGOTIATION_REJECTED};

  for (auto status : failures) {
    provider.answer = status;
    subject.bind<Abstract>().visit([&](Abstract) { EXPECT(False); }, [&] {});
  }

  EXPECT_EQ(provider.binds, Count(4));
}

// A route returns None, while support on the original Abstract can report
// Rejected or Unknown. Binding still calls that original Abstract's operation.
// Its policy governs negotiation independently of the route result.
VALIDATION_TEST(Abstracts, encountered_policy) {
  struct Policy {
    ttx_abstract target;
    ttx_negotiation_status answer;
    Count binds = 0;

    static auto state(void* context) -> ttx_abstract_state {
      return TTX_ABSTRACT_PRESENT;
    }

    static auto supports(void* context, perimortem_uuid id)
        -> ttx_negotiation_status {
      if (id.high == TTX_NONE_ID_HIGH &&
          (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
        return TTX_NEGOTIATION_REJECTED;
      }

      return Data::cast<Policy>(context)->answer;
    }

    static auto bind(void* context, perimortem_uuid id, void* output)
        -> ttx_negotiation_status {
      ++Data::cast<Policy>(context)->binds;
      return TTX_NEGOTIATION_REJECTED;
    }

    static auto lookup(void* context, perimortem_view_bytes route)
        -> ttx_abstract {
      return Data::cast<Policy>(context)->target;
    }

    static auto visit(void* context, ttx_graph_visitor callback) -> void {}
  } policy{ttx_none(), TTX_NEGOTIATION_REJECTED};

  const ttx_abstract_ops operations = {
    Policy::supports, Policy::bind, Policy::lookup, Policy::visit,
    Policy::state};

  const Abstract subject(&policy, operations);
  EXPECT(subject.resolve_route("selected"_view) == Abstract(ttx_none()));
  EXPECT(
      subject.supports<Policies::Constant>() == Negotiation::Status::Rejected);

  policy.answer = TTX_NEGOTIATION_UNKNOWN;
  EXPECT(
      subject.supports<Policies::Constant>() == Negotiation::Status::Unknown);
  EXPECT_EQ(policy.binds, Count(0));

  subject.bind<Policies::Constant>().visit(
      [&](Policies::Constant) { EXPECT(False); }, [&] {});

  EXPECT_EQ(policy.binds, Count(1));
}

// A combined contract has its own UUID. Its provider can establish that
// contract while leaving separately requested constituent UUIDs undetermined.
VALIDATION_TEST(Abstracts, qualified_question) {
  struct Subject {
    Count probes = 0;

    static auto contract() -> Formats::Uuid {
      return Formats::Uuid(0x274605faccbb48b0ULL, 0x89c84603372550d1ULL);
    }

    static auto supports(void* context, perimortem_uuid id)
        -> ttx_negotiation_status {
      ++Data::cast<Subject>(context)->probes;
      return Formats::Uuid(id) == contract() ? TTX_NEGOTIATION_SATISFIED
                                             : TTX_NEGOTIATION_UNKNOWN;
    }

    static auto bind(void* context, perimortem_uuid id, ttx_storage target)
        -> ttx_negotiation_status {
      if (Formats::Uuid(id) != contract()) {
        return TTX_NEGOTIATION_UNKNOWN;
      }

      Abstract::Api api = ttx_none();
      return Transfer::copy(Form::Storage::from(api), Form::Storage(target)) ==
                     Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }
  } provider;

  const Negotiation::Query query({&provider, Subject::bind, Subject::supports});
  EXPECT(query.supports<Policies::Ordered>() == Negotiation::Status::Unknown);

  const auto probes = provider.probes;
  query.bind<Abstract>(Subject::contract())
      .visit(
          [&](Abstract answer) { EXPECT(answer == Abstract(ttx_none())); },
          [&](Negotiation::Status) { EXPECT(False); });
  EXPECT_EQ(provider.probes, probes);
}

// Ordinary binding outcomes can change while the provider remains Present.
// The two reserved UUIDs retain their explicit rejection throughout.
VALIDATION_TEST(Abstracts, reserved_bindings) {
  observation_subject provider{};
  const auto api = observation_abstract(&provider);
  const ttx_negotiation_status statuses[] = {
    TTX_NEGOTIATION_SATISFIED, TTX_NEGOTIATION_UNKNOWN,
    TTX_NEGOTIATION_REJECTED};

  const perimortem_uuid ids[] = {
    {TTX_NONE_ID_HIGH, TTX_NONE_ID_LOW},
    {TTX_UNKNOWN_ID_HIGH, TTX_UNKNOWN_ID_LOW},
  };

  for (const auto status : statuses) {
    provider.answer = status;
    EXPECT_EQ(api.operations->get_state(api.context), TTX_ABSTRACT_PRESENT);
    for (const auto id : ids) {
      EXPECT_EQ(
          api.operations->supports(api.context, id), TTX_NEGOTIATION_REJECTED);

      ttx_abstract output{};
      EXPECT_EQ(
          api.operations->bind(api.context, id, &output),
          TTX_NEGOTIATION_REJECTED);
    }
  }
}
