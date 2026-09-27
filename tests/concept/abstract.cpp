// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/null_terminated.hpp"

#include "tests/concept/fixtures/observation.h"
#include "tests/concept/fixtures/subject.hpp"
#include "tests/library.hpp"
#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/answers/constant.hpp"
#include "ttx/concept/answers/none.hpp"

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
  observation_subject owner{
    &Binding::representation<Abstract>(),
    &Binding::representation<Answers::Constant>(),
    0,
  };
  const Abstract subject(observation_abstract(&owner));
  subject.get_query().bind<Abstract>().visit(
      [&](Abstract acquired) { EXPECT(acquired == subject); },
      [&](Binding::Failure) { EXPECT(False); });
  subject.bind<Answers::Constant>().visit(
      [](Answers::Constant) {}, [&](Binding::Failure) { EXPECT(False); });

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
  EXPECT(subject.resolve_concept("missing"_view) == Answers::None::get_none());
}

VALIDATION_TEST(Abstracts, local_cast) {
  const Subject owner;
  const auto subject = Abstract::provide(owner);
  const auto local = subject.cast<Subject>();
  ASSERT(local);
  EXPECT(&*local == &owner);
  EXPECT_NOT(subject.cast<Requirement>());
  static_assert(__is_same(decltype(*local), const Subject&));
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
  struct Owner {
    auto get_data() const -> Core::View::Bytes { return "provider"_view; }
    auto supports(System::Uuid id) const -> Binding::Status {
      return id == Answers::Constant::contract_id ? Binding::Status::Satisfied
                                                  : Binding::Status::Rejected;
    }
  } owner;
  const auto subject = Abstract::provide(owner);
  struct View : Abstract {
    explicit View(Abstract subject) : Abstract(subject) {}
  } view(subject);
  EXPECT(Abstract::provide(view) == subject);
  EXPECT(view.supports<Answers::Constant>() == Binding::Status::Satisfied);
  EXPECT(view.supports<Answers::None>() == Binding::Status::Rejected);
  EXPECT(view.cast<Owner>());
  EXPECT_NOT(view.cast<View>());
}
