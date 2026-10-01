// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/constant.hpp"

#include "perimortem/core/null_terminated.hpp"

#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/unknown.hpp"

using namespace Perimortem;
using namespace Ttx;
static Toolchain::Validation::Harness Constants = {.name = "TTX::Constant"};

// Constant qualifies the incoming edge. Forwarding a subject's other answers
// through this policy does not make those answers Constant as well.
VALIDATION_TEST(Constants, explicit_constant_policy) {
  using Concept::Abstract;
  using namespace Semantic::Negotiation;
  struct Fixed {
    Abstract target;
    auto get_data() const -> Core::View::Bytes { return target.get_data(); }
    auto supports(System::Uuid id) const -> Binding::Status {
      return id == Concept::Policies::Constant::contract_id
                 ? Binding::Status::Satisfied
                 : target.supports(id);
    }
    auto bind_interface(System::Uuid id, Data::Form::Storage output)
        -> Binding::Status {
      if (id == Concept::Policies::Constant::contract_id ||
          ((id == Concept::Policies::None::contract_id ||
            id == Concept::Policies::Unknown::contract_id) &&
           target.supports(id) == Binding::Status::Satisfied)) {
        return Binding::provide<Abstract>(
            Abstract::provide(*this).get_abi(), output);
      }
      return target.bind_interface(id, output);
    }
    auto resolve_concept(Core::View::Bytes route) const -> Abstract {
      return target.resolve_concept(route);
    }
  };
  Fixed rejected(Concept::Policies::None::get_none());
  const auto permanent = Abstract::provide(rejected);
  permanent.bind<Concept::Policies::None>().visit(
      [&](Concept::Policies::None answer) {
        EXPECT(
            answer.supports<Concept::Policies::Constant>() ==
            Binding::Status::Satisfied);
        EXPECT(answer.get_identity() == permanent.get_identity());
      },
      [&](Binding::Failure) { EXPECT(False); });
  permanent.bind<Concept::Policies::Constant>().visit(
      [](Concept::Policies::Constant) {},
      [&](Binding::Failure) { EXPECT(False); });
  EXPECT(permanent.resolve() == permanent);

  struct Mutable {
    Abstract child;
    auto get_data() const -> Core::View::Bytes { return Core::View::Bytes(); }
    auto resolve_concept(Core::View::Bytes) const -> Abstract { return child; }
  } provider(Concept::Policies::Unknown::get_unknown());
  Fixed fixed(Abstract::provide(provider));
  const auto edge = Abstract::provide(fixed);
  EXPECT(
      edge.supports<Concept::Policies::Constant>() ==
      Binding::Status::Satisfied);
  EXPECT(edge.resolve_concept("child"_view) == provider.child);
  provider.child = Concept::Policies::None::get_none();
  const auto next = edge.resolve_concept("child"_view);
  EXPECT(next == provider.child);
  EXPECT(
      next.supports<Concept::Policies::Constant>() == Binding::Status::Unknown);
}
