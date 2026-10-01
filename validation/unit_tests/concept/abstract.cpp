// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/null_terminated.hpp"

#include "validation/unit_tests/concept/fixtures/observation.h"
#include "validation/unit_tests/concept/fixtures/subject.hpp"
#include "validation/unit_tests/library.hpp"
#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/policies/constant.hpp"
#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/ordered.hpp"
#include "ttx/concept/policies/unknown.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;
using namespace Validation::ConceptTests;

static Toolchain::Validation::Harness Abstracts = {
  .name = "TTX::Abstract",
};

struct Requirement {
  U8 family;
  auto get_data() const -> Core::View::Bytes {
    return Core::View::Bytes(&family, 1);
  }
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
  auto receive = [&](Core::View::Bytes route, Abstract value) {
    ++visited;
    EXPECT_EQ(route.get_size(), Count(3));
    EXPECT_EQ(route.get_data()[1], U8(0));
    EXPECT(subject.resolve_concept(route) == value);
  };
  subject.visit_concepts(Abstract::Visitor(receive));
  EXPECT_EQ(visited, Count(1));
  EXPECT(subject.resolve_concept("missing"_view) == Policies::None::get_none());
}

VALIDATION_TEST(Abstracts, local_cast) {
  Subject provider;
  const auto subject = Abstract::provide(provider);
  const auto local = subject.cast<Subject>();
  ASSERT(local);
  EXPECT(&*local == &provider);
  EXPECT_NOT(subject.cast<Requirement>());
  static_assert(__is_same(decltype(*local), Subject&));
  static_assert(sizeof(Abstract) == sizeof(ttx_abstract));
}

VALIDATION_TEST(Abstracts, foreign_same_type) {
  auto library = Validation::open_library("libabstract_subject.so"_view);
  const auto open = reinterpret_cast<ttx_abstract (*)()>(
      Validation::find_symbol(library, "abstract_subject"_view));
  const Abstract foreign(open());
  const Subject local;
  EXPECT_NOT(foreign.cast<Subject>());
  EXPECT(foreign.get_data() == local.get_data());
  EXPECT(foreign.resolve() == foreign);
  foreign.get_query().bind<Abstract>().visit(
      [&](Abstract acquired) { EXPECT(acquired == foreign); },
      [&](Binding::Failure) { EXPECT(False); });
}

// A provider implements the UUID question directly. Acquiring a richer C++
// view over its publication must not republish the view as a second subject or
// mistake consumer forwarding methods for provider operations.
VALIDATION_TEST(Abstracts, native_supports) {
  struct Provider {
    auto get_data() const -> Core::View::Bytes { return "provider"_view; }
    auto supports(System::Uuid id) const -> Binding::Status {
      return id == Policies::Constant::contract_id ? Binding::Status::Satisfied
                                                   : Binding::Status::Rejected;
    }
  } provider;
  const auto subject = Abstract::provide(provider);
  struct View : Abstract {
    explicit View(Abstract subject) : Abstract(subject) {}
  } view(subject);
  EXPECT(Abstract::provide(view) == subject);
  EXPECT(view.supports<Policies::Constant>() == Binding::Status::Satisfied);
  EXPECT(view.supports<Policies::None>() == Binding::Status::Rejected);
  EXPECT(view.cast<Provider>());
  EXPECT_NOT(view.cast<View>());
}

// Optional native operations may be overloaded without changing the portable
// call signature. Each lookup must select the callable overload rather than
// mistaking an overloaded name for an absent operation.
VALIDATION_TEST(Abstracts, overloaded_provider) {
  struct Provider {
    auto get_data() const -> Core::View::Bytes { return Core::View::Bytes(); }
    auto bind_interface(System::Uuid, Ttx::Data::Form::Storage output)
        -> Binding::Status {
      return Binding::provide<Policies::Unknown>(
          Abstract::provide(*this).get_abi(), output);
    }
    auto bind_interface(System::Uuid) const -> Binding::Status {
      return Binding::Status::Unknown;
    }
    auto resolve() const -> Abstract {
      return Policies::Unknown::get_unknown();
    }
    auto resolve(Count) const -> Abstract { return Policies::None::get_none(); }
    auto resolve_concept(Core::View::Bytes) const -> Abstract {
      return Policies::Unknown::get_unknown();
    }
    auto resolve_concept(Count) const -> Abstract {
      return Policies::None::get_none();
    }
    auto visit_concepts(Abstract::Visitor visitor) const -> void {
      visitor("query"_view, Policies::Unknown::get_unknown());
    }
    auto visit_concepts(Count) const -> void {}
  } provider;
  const auto subject = Abstract::provide(provider);
  subject.bind<Policies::Unknown>().visit(
      [&](Policies::Unknown) {}, [&](Binding::Failure) { EXPECT(False); });
  EXPECT(subject.resolve() == Policies::Unknown::get_unknown());
  EXPECT(
      subject.resolve_concept("query"_view) ==
      Policies::Unknown::get_unknown());
  Count visited = 0;
  auto receive = [&](Core::View::Bytes route, Abstract value) {
    EXPECT(route == "query"_view);
    EXPECT(value == Policies::Unknown::get_unknown());
    ++visited;
  };
  subject.visit_concepts(Abstract::Visitor(receive));
  EXPECT_EQ(visited, Count(1));
}

// A complete question can have an answer even when this provider has no
// vocabulary for the consumer's proposed constituent questions.
VALIDATION_TEST(Abstracts, qualified_question) {
  using namespace Ttx;
  const System::Uuid qualified =
      System::Uuid(0x274605faccbb48b0ULL, 0x89c84603372550d1ULL);
  struct Subject {
    System::Uuid qualified;
    Count probes = 0;
    auto get_data() const -> Core::View::Bytes { return Core::View::Bytes(); }
    auto supports(System::Uuid id) -> Binding::Status {
      ++probes;
      return id == qualified ? Binding::Status::Satisfied
                             : Binding::Status::Unknown;
    }
    auto bind_interface(System::Uuid id, Data::Form::Storage target)
        -> Binding::Status {
      return id == qualified ? Binding::provide<Abstract>(
                                   Abstract::provide(*this).get_abi(), target)
                             : Binding::Status::Unknown;
    }
  } provider{qualified};
  const auto subject = Concept::Abstract::provide(provider);
  EXPECT(
      subject.supports<Concept::Policies::Ordered>() ==
      Binding::Status::Unknown);
  const auto probed = provider.probes;
  ttx_abstract answer = ttx_abstract();
  const Data::Form::Storage output(
      {&Binding::representation<Concept::Abstract>(),
       reinterpret_cast<U8*>(&answer), sizeof(answer)});
  EXPECT(
      subject.bind_interface(qualified, output) == Binding::Status::Satisfied);
  EXPECT(Concept::Abstract(answer) == subject);
  EXPECT_EQ(provider.probes, probed);
}

// Omitting lookup leaves route questions undetermined. A provider establishes
// absence by implementing that question and returning None explicitly.
VALIDATION_TEST(Abstracts, omitted_lookup_is_unknown) {
  struct Provider {
    auto get_data() const -> Core::View::Bytes { return Core::View::Bytes(); }
  } provider;
  const auto answer =
      Abstract::provide(provider).resolve_concept("unrecognized"_view);
  EXPECT(answer.supports<Policies::Unknown>() == Binding::Status::Satisfied);
  EXPECT(answer.supports<Policies::None>() == Binding::Status::Unknown);
}
