// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/support/library.hpp"

#include "perimortem/core/null_terminated.hpp"

#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/policies/constant.hpp"
#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/ordered.hpp"
#include "ttx/concept/policies/unknown.hpp"
#include "validation/unit_tests/concept/fixtures/observation.h"

using namespace Perimortem;
using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

static Toolchain::Validation::Harness Abstracts = {
  .name = "TTX::Abstract",
};

VALIDATION_TEST(Abstracts, c_observations) {
  observation_subject provider = observation_subject(
      &Binding::representation<Abstract>(),
      &Binding::representation<Policies::Constant>(), 0);
  const Abstract subject(observation_abstract(&provider));
  subject.get_query().bind<Abstract>().visit(
      [&](Abstract acquired) { EXPECT(acquired == subject); },
      [&](Binding::Failure) { EXPECT(False); });
  subject.bind<Policies::Constant>().visit(
      [](Policies::Constant) {}, [&](Binding::Failure) { EXPECT(False); });

  // Save the byte itself, since a new observation ends the first view's
  // retention promise. A Constant marker does not change that rule.
  const U8 first = subject.get_data().get_data()[0];
  const U8 second = subject.get_data().get_data()[0];
  EXPECT_EQ(first, U8(1));
  EXPECT_EQ(second, U8(2));
  EXPECT(subject.resolve().resolve() == subject);

  Count visited = 0;
  auto callback = [&](Core::View::Bytes route, Abstract value) {
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
      &Binding::representation<Abstract>(),
      &Binding::representation<Policies::Constant>(), 0);
  const Abstract subject(observation_abstract(&provider));
  const auto copy = subject;
  EXPECT(copy == subject);
  EXPECT_EQ(subject.get_data()[0], U8(1));
  EXPECT_EQ(copy.get_data()[0], U8(2));
  EXPECT_EQ(subject.get_data()[0], U8(3));
  static_assert(sizeof(Abstract) == sizeof(ttx_abstract));
}

VALIDATION_TEST(Abstracts, foreign_publication) {
  auto library = Validation::open_library("libabstract_subject.so"_view);
  const auto open = reinterpret_cast<ttx_abstract (*)()>(
      Validation::find_symbol(library, "abstract_subject"_view));
  const Abstract foreign(open());
  EXPECT(foreign.get_data() == "subject"_view);
  EXPECT(foreign.resolve() == foreign);
  foreign.bind<Abstract>().visit(
      [&](Abstract acquired) { EXPECT(acquired == foreign); },
      [&](Binding::Failure) { EXPECT(False); });
}

// The discovery context is not the bound context. Even an Abstract request
// goes back to the provider, which may select another answer or refuse it.
VALIDATION_TEST(Abstracts, fresh_abstract_binding) {
  observation_subject provider = observation_subject(
      &Binding::representation<Abstract>(),
      &Binding::representation<Policies::Constant>(), 0);
  const Abstract subject(observation_abstract(&provider));
  const auto check = [&](Abstract expected) {
    subject.bind<Abstract>().visit(
        [&](Abstract answer) { EXPECT(answer == expected); },
        [&](Binding::Failure) { EXPECT(False); });
  };
  provider.selected = ttx_none();
  check(Abstract(ttx_none()));

  provider.selected = ttx_unknown();
  check(Abstract(ttx_unknown()));

  const ttx_binding_status failures[] = {
    TTX_BINDING_UNKNOWN, TTX_BINDING_REJECTED};
  for (auto status : failures) {
    provider.answer = status;
    subject.bind<Abstract>().visit(
        [&](Abstract) { EXPECT(False); },
        [&](Binding::Failure failure) {
          EXPECT_EQ(static_cast<ttx_binding_status>(failure), status);
        });
  }

  EXPECT_EQ(provider.binds, Count(4));
}

// Resolving this layer exposes a different subject. Support and binding must
// still use this layer's answers, including its explicit refusal.
VALIDATION_TEST(Abstracts, encountered_policy) {
  struct Policy {
    ttx_abstract target;
    ttx_binding_status answer;
    Count binds = 0;

    static auto supports(void* context, perimortem_uuid) -> ttx_binding_status {
      return static_cast<Policy*>(context)->answer;
    }

    static auto bind(void* context, perimortem_uuid, ttx_storage)
        -> ttx_binding_status {
      ++static_cast<Policy*>(context)->binds;
      return TTX_BINDING_REJECTED;
    }

    static auto data(void*) -> perimortem_view_bytes { return {}; }

    static auto resolve(void* context) -> ttx_abstract {
      return static_cast<Policy*>(context)->target;
    }

    static auto lookup(void*, perimortem_view_bytes) -> ttx_abstract {
      return ttx_unknown();
    }

    static void visit(void*, ttx_concept_visitor) {}
  } policy{ttx_none(), TTX_BINDING_REJECTED};
  const ttx_abstract_ops operations = {Policy::supports, Policy::bind,
                                       Policy::data,     Policy::resolve,
                                       Policy::lookup,   Policy::visit};
  const Abstract subject(&policy, operations);
  EXPECT(subject.resolve() == Abstract(ttx_none()));
  EXPECT(subject.supports<Policies::None>() == Binding::Status::Rejected);

  policy.answer = TTX_BINDING_UNKNOWN;
  EXPECT(subject.supports<Policies::None>() == Binding::Status::Unknown);
  EXPECT_EQ(policy.binds, Count(0));

  subject.bind<Policies::None>().visit(
      [&](Policies::None) { EXPECT(False); },
      [&](Binding::Failure failure) {
        EXPECT(failure == Binding::Failure::Rejected);
      });
  EXPECT_EQ(policy.binds, Count(1));
}

// A combined promise is one question. Its provider need not know the
// consumer's vocabulary for any proposed constituent promise.
VALIDATION_TEST(Abstracts, qualified_question) {
  struct Subject {
    Count probes = 0;

    static auto contract() -> System::Uuid {
      return System::Uuid(0x274605faccbb48b0ULL, 0x89c84603372550d1ULL);
    }

    static auto supports(void* context, perimortem_uuid id)
        -> ttx_binding_status {
      ++static_cast<Subject*>(context)->probes;
      return System::Uuid(id) == contract() ? TTX_BINDING_SATISFIED
                                            : TTX_BINDING_UNKNOWN;
    }

    static auto bind(void*, perimortem_uuid id, ttx_storage target)
        -> ttx_binding_status {
      if (System::Uuid(id) != contract()) {
        return TTX_BINDING_UNKNOWN;
      }

      return static_cast<ttx_binding_status>(Binding::provide<Abstract>(
          ttx_none(), Ttx::Data::Form::Storage(target)));
    }
  } provider;
  const Query query({&provider, Subject::bind, Subject::supports});
  EXPECT(query.supports<Policies::Ordered>() == Binding::Status::Unknown);

  const auto probes = provider.probes;
  query.bind<Abstract>(Subject::contract())
      .visit(
          [&](Abstract answer) { EXPECT(answer == Abstract(ttx_none())); },
          [&](Binding::Failure) { EXPECT(False); });
  EXPECT_EQ(provider.probes, probes);
}
