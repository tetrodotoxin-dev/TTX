// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/constant.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/unknown.hpp"
#include "ttx/data/transfer.hpp"

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
  U8 value = 17;

  static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
    return Serialization::Formats::Uuid(id) == Abstract::contract_id
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_binding_status {
    if (supports(context, id) != TTX_BINDING_SATISFIED) {
      return TTX_BINDING_UNKNOWN;
    }

    Abstract::Api api = resolve(context);
    return Ttx::Data::Transfer::copy(
               Ttx::Data::Form::Storage::from(api),
               Data::Form::Storage(output)) == Ttx::Data::Status::Success
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_REJECTED;
  }

  static auto data(void* context) -> perimortem_view_bytes {
    return {&static_cast<Changing*>(context)->value, 1};
  }

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
    if (Serialization::Formats::Uuid(id) ==
        Concept::Policies::Constant::contract_id) {
      return TTX_BINDING_SATISFIED;
    }

    const auto target = static_cast<Fixed*>(context)->target;
    return target.operations->supports(target.context, id);
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_binding_status {
    const Serialization::Formats::Uuid contract(id);
    if (contract == Abstract::contract_id ||
        contract == Concept::Policies::Constant::contract_id ||
        ((contract == Concept::Policies::None::contract_id ||
          contract == Concept::Policies::Unknown::contract_id) &&
         supports(context, id) == TTX_BINDING_SATISFIED)) {
      Abstract::Api api = resolve(context);
      return Ttx::Data::Transfer::copy(
                 Ttx::Data::Form::Storage::from(api),
                 Data::Form::Storage(output)) == Ttx::Data::Status::Success
                 ? TTX_BINDING_SATISFIED
                 : TTX_BINDING_REJECTED;
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

      [&](Binding::Status) { EXPECT(False); });
  permanent.bind<Concept::Policies::Constant>().visit(
      [](Concept::Policies::Constant) {},
      [&](Binding::Status) { EXPECT(False); });
  EXPECT(permanent.resolve() == permanent);

  Changing provider{ttx_unknown()};
  Fixed fixed{Changing::resolve(&provider)};
  Changing parent{Fixed::resolve(&fixed)};
  const Abstract root(Changing::resolve(&parent));
  const auto first = root.resolve_concept("selected"_view);
  EXPECT(
      first.supports<Concept::Policies::Constant>() ==
      Binding::Status::Satisfied);
  EXPECT_EQ(first.get_data()[0], U8(17));
  EXPECT(first.resolve_concept("child"_view) == Abstract(ttx_unknown()));

  provider.value = 92;
  provider.child = ttx_none();

  const auto second = root.resolve_concept("selected"_view);
  EXPECT(first == second);
  EXPECT_EQ(second.get_data()[0], U8(92));

  const auto child = second.resolve_concept("child"_view);
  EXPECT(child == Abstract(ttx_none()));
  EXPECT(
      child.supports<Concept::Policies::Constant>() ==
      Binding::Status::Unknown);
}

// The shared None answer does not freeze the original provider's selection.
VALIDATION_TEST(Constants, rejection_can_change) {
  Changing provider{ttx_none()};
  const Abstract subject(Changing::resolve(&provider));
  EXPECT(subject.resolve_concept("child"_view) == Abstract(ttx_none()));

  provider.child = Changing::resolve(&provider);
  EXPECT(subject.resolve_concept("child"_view) == subject);
}
