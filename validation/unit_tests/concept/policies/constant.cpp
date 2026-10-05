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

namespace {

using Concept::Abstract;
namespace Binding = Semantic::Negotiation::Binding;

// This provider exposes one changing outgoing edge. The consumer keeps the
// provider's identity while asking it again for each child observation.
struct Changing {
  ttx_abstract child;

  static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
    return System::Uuid(id) == Abstract::contract_id ? TTX_BINDING_SATISFIED
                                                     : TTX_BINDING_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_binding_status {
    if (supports(context, id) != TTX_BINDING_SATISFIED) {
      return TTX_BINDING_UNKNOWN;
    }

    return static_cast<ttx_binding_status>(Binding::provide<Abstract>(
        resolve(context), Data::Form::Storage(output)));
  }

  static auto data(void*) -> perimortem_view_bytes { return {}; }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations};
  }

  static auto lookup(void* context, perimortem_view_bytes) -> ttx_abstract {
    return static_cast<Changing*>(context)->child;
  }

  static void visit(void*, ttx_concept_visitor) {}

  static constexpr ttx_abstract_ops operations = {supports, bind,   data,
                                                  resolve,  lookup, visit};
};

// Fixed preserves the selected answer. Its outgoing routes still use the
// target's operations and retain the promises supplied by those answers.
struct Fixed {
  ttx_abstract target;

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_binding_status {
    if (System::Uuid(id) == Concept::Policies::Constant::contract_id) {
      return TTX_BINDING_SATISFIED;
    }

    const auto target = static_cast<Fixed*>(context)->target;
    return target.operations->supports(target.context, id);
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_binding_status {
    const System::Uuid contract(id);
    if (contract == Abstract::contract_id ||
        contract == Concept::Policies::Constant::contract_id ||
        ((contract == Concept::Policies::None::contract_id ||
          contract == Concept::Policies::Unknown::contract_id) &&
         supports(context, id) == TTX_BINDING_SATISFIED)) {
      return static_cast<ttx_binding_status>(Binding::provide<Abstract>(
          resolve(context), Data::Form::Storage(output)));
    }

    const auto target = static_cast<Fixed*>(context)->target;
    return target.operations->bind(target.context, id, output);
  }

  static auto data(void* context) -> perimortem_view_bytes {
    const auto target = static_cast<Fixed*>(context)->target;
    return target.operations->get_data(target.context);
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    const auto target = static_cast<Fixed*>(context)->target;
    return target.operations->resolve_concept(target.context, route);
  }

  static void visit(void* context, ttx_concept_visitor callback) {
    const auto target = static_cast<Fixed*>(context)->target;
    target.operations->visit_concepts(target.context, callback);
  }

  static constexpr ttx_abstract_ops operations = {supports, bind,   data,
                                                  resolve,  lookup, visit};
};

}  // namespace

VALIDATION_TEST(Constants, explicit_constant_policy) {
  Fixed rejected{ttx_none()};
  const Abstract permanent(Fixed::resolve(&rejected));
  permanent.bind<Concept::Policies::None>().visit(
      [&](Concept::Policies::None answer) {
        EXPECT(
            answer.get_abstract().supports<Concept::Policies::Constant>() ==
            Binding::Status::Satisfied);
        EXPECT(answer.get_abstract() == permanent);
      },
      [&](Binding::Failure) { EXPECT(False); });
  permanent.bind<Concept::Policies::Constant>().visit(
      [](Concept::Policies::Constant) {},
      [&](Binding::Failure) { EXPECT(False); });
  EXPECT(permanent.resolve() == permanent);

  Changing provider{ttx_unknown()};
  Fixed fixed{Changing::resolve(&provider)};
  const Abstract edge(Fixed::resolve(&fixed));
  EXPECT(
      edge.supports<Concept::Policies::Constant>() ==
      Binding::Status::Satisfied);
  EXPECT(edge.resolve_concept("child"_view) == Abstract(provider.child));

  provider.child = ttx_none();
  const auto next = edge.resolve_concept("child"_view);
  EXPECT(next == Abstract(provider.child));
  EXPECT(
      next.supports<Concept::Policies::Constant>() == Binding::Status::Unknown);
}

// The shared None answer does not freeze the original provider's selection.
VALIDATION_TEST(Constants, rejection_can_change) {
  Changing provider{ttx_none()};
  const Abstract subject(Changing::resolve(&provider));
  EXPECT(subject.resolve_concept("child"_view) == Abstract(ttx_none()));

  provider.child = Changing::resolve(&provider);
  EXPECT(subject.resolve_concept("child"_view) == subject);
}
