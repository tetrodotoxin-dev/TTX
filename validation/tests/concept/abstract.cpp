// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/concept/policies/constant.hpp"
#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/ordered.hpp"
#include "ttx/concept/policies/unknown.hpp"
#include "ttx/data/transfer.hpp"
#include "validation/tests/concept/fixtures/observation.h"

using namespace Perimortem::Core;
using namespace Perimortem::Serialization;
using namespace Toolchain::Validation;
using namespace Ttx::Concept;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness Abstracts = {
  .name = "TTX::Abstract",
};

VALIDATION_TEST(Abstracts, c_observations) {
  observation_subject provider = observation_subject(
      &Form::Compiled<
           Form::Native<Abstract::Api>::reference>::get_representation()
           .get_abi(),
      &Form::Compiled<Form::Native<Policies::Constant::Api>::reference>::
           get_representation()
               .get_abi(),
      0);
  const Abstract subject(observation_abstract(&provider));
  subject.get_query().bind<Abstract>().visit(
      [&](Abstract acquired) { EXPECT(acquired == subject); },
      [&](Negotiation::Status) { EXPECT(False); });
  subject.bind<Policies::Constant>().visit(
      [](Policies::Constant) {}, [&](Negotiation::Status) { EXPECT(False); });

  // Copy the first byte before asking for another observation, which can
  // replace the provider's buffer. Constant keeps the selection stable while
  // these byte observations can still change.
  const U8 first = subject.get_data().get_data()[0];
  const U8 second = subject.get_data().get_data()[0];
  EXPECT_EQ(first, U8(1));
  EXPECT_EQ(second, U8(2));
  EXPECT(subject.resolve().resolve() == subject);

  Count visited = 0;
  auto callback = [&](View::Bytes route, Abstract value) {
    ++visited;
    EXPECT_EQ(route.get_size(), Count(3));
    EXPECT_EQ(route.get_data()[1], U8(0));
    EXPECT(subject.resolve_concept(route) == value);
  };

  subject.visit_concepts(Abstract::Visitor(callback));
  EXPECT_EQ(visited, Count(1));
  EXPECT(
      subject.resolve_concept("missing"_view) ==
      Policies::None::get_none().get_abstract());
}

// Copying a const handle preserves its operations. It imposes no constness on
// the provider, which makes a fresh byte observation on every call.
VALIDATION_TEST(Abstracts, copied_view) {
  observation_subject provider = observation_subject(
      &Form::Compiled<
           Form::Native<Abstract::Api>::reference>::get_representation()
           .get_abi(),
      &Form::Compiled<Form::Native<Policies::Constant::Api>::reference>::
           get_representation()
               .get_abi(),
      0);
  const Abstract subject(observation_abstract(&provider));
  const auto copy = subject;
  EXPECT(copy == subject);
  EXPECT_EQ(subject.get_data()[0], U8(1));
  EXPECT_EQ(copy.get_data()[0], U8(2));
  EXPECT_EQ(subject.get_data()[0], U8(3));

  static_assert(sizeof(Abstract) == sizeof(ttx_abstract));
}

VALIDATION_TEST(Abstracts, fresh_abstract_binding) {
  observation_subject provider = observation_subject(
      &Form::Compiled<
           Form::Native<Abstract::Api>::reference>::get_representation()
           .get_abi(),
      &Form::Compiled<Form::Native<Policies::Constant::Api>::reference>::
           get_representation()
               .get_abi(),
      0);
  const Abstract subject(observation_abstract(&provider));
  const auto check = [&](Abstract expected) {
    subject.bind<Abstract>().visit(
        [&](Abstract answer) { EXPECT(answer == expected); },
        [&](Negotiation::Status) { EXPECT(False); });
  };

  provider.selected = ttx_none();
  check(Abstract(ttx_none()));

  provider.selected = ttx_unknown();
  check(Abstract(ttx_unknown()));

  const ttx_negotiation_status failures[] = {
    TTX_NEGOTIATION_UNKNOWN, TTX_NEGOTIATION_REJECTED};

  for (auto status : failures) {
    provider.answer = status;
    subject.bind<Abstract>().visit(
        [&](Abstract) { EXPECT(False); },
        [&](Negotiation::Status failure) {
          EXPECT_EQ(ttx_negotiation_status(failure), status);
        });
  }

  EXPECT_EQ(provider.binds, Count(4));
}

// Resolution returns None, while support on the original Abstract can report
// Rejected or Unknown. Binding still calls that original Abstract's operation.
// Its policy governs negotiation independently of the result of resolution.
VALIDATION_TEST(Abstracts, encountered_policy) {
  struct Policy {
    ttx_abstract target;
    ttx_negotiation_status answer;
    Count binds = 0;

    static auto supports(void* context, perimortem_uuid id)
        -> ttx_negotiation_status {
      return Data::cast<Policy>(context)->answer;
    }

    static auto bind(void* context, perimortem_uuid, ttx_storage)
        -> ttx_negotiation_status {
      ++Data::cast<Policy>(context)->binds;
      return TTX_NEGOTIATION_REJECTED;
    }

    static auto data(void* context) -> perimortem_view_bytes { return {}; }

    static auto resolve(void* context) -> ttx_abstract {
      return Data::cast<Policy>(context)->target;
    }

    static auto lookup(void* context, perimortem_view_bytes route)
        -> ttx_abstract {
      return ttx_unknown();
    }

    static auto visit(void* context, ttx_concept_visitor callback) -> void {}
  } policy{ttx_none(), TTX_NEGOTIATION_REJECTED};

  const ttx_abstract_ops operations = {Policy::supports, Policy::bind,
                                       Policy::data,     Policy::resolve,
                                       Policy::lookup,   Policy::visit};

  const Abstract subject(&policy, operations);
  EXPECT(subject.resolve() == Abstract(ttx_none()));
  EXPECT(subject.supports<Policies::None>() == Negotiation::Status::Rejected);

  policy.answer = TTX_NEGOTIATION_UNKNOWN;
  EXPECT(subject.supports<Policies::None>() == Negotiation::Status::Unknown);
  EXPECT_EQ(policy.binds, Count(0));

  subject.bind<Policies::None>().visit(
      [&](Policies::None) { EXPECT(False); },
      [&](Negotiation::Status failure) {
        EXPECT(failure == Negotiation::Status::Rejected);
      });

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
