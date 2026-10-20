// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/constant.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/unknown.hpp"
#include "ttx/data/transfer.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Serialization;
using namespace Toolchain::Validation;
using namespace Ttx::Concept;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness Constants = {.name = "TTX::Constant"};

// This provider exposes one changing outgoing edge. The consumer keeps the
// provider's identity while asking it again for each child observation.
struct Changing {
  ttx_abstract child;
  U8 value = 17;

  static auto supports(void*, perimortem_uuid id) -> ttx_negotiation_status {
    return Formats::Uuid(id) == Abstract::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_negotiation_status {
    if (supports(context, id) != TTX_NEGOTIATION_SATISFIED) {
      return TTX_NEGOTIATION_UNKNOWN;
    }

    Abstract::Api api = resolve(context);
    return Transfer::copy(Form::Storage::from(api), Form::Storage(output)) ==
                   Status::Success
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  static auto data(void* context) -> perimortem_view_bytes {
    return {&Data::cast<Changing>(context)->value, 1};
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations};
  }

  static auto lookup(void* context, perimortem_view_bytes) -> ttx_abstract {
    return Data::cast<Changing>(context)->child;
  }

  static auto visit(void*, ttx_concept_visitor) -> void {}

  static constexpr ttx_abstract_ops operations = {supports, bind,   data,
                                                  resolve,  lookup, visit};
};

// Fixed preserves the selected Abstract. Outgoing routes still use the
// target's operations and carry their own policies independently of Constant.
struct Fixed {
  ttx_abstract target;

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (Formats::Uuid(id) == Policies::Constant::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    const auto target = Data::cast<Fixed>(context)->target;
    return target.operations->supports(target.context, id);
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_negotiation_status {
    const Formats::Uuid contract(id);
    if (contract == Abstract::contract_id ||
        contract == Policies::Constant::contract_id ||
        ((contract == Policies::None::contract_id ||
          contract == Policies::Unknown::contract_id) &&
         supports(context, id) == TTX_NEGOTIATION_SATISFIED)) {
      Abstract::Api api = resolve(context);
      return Transfer::copy(Form::Storage::from(api), Form::Storage(output)) ==
                     Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    const auto target = Data::cast<Fixed>(context)->target;
    return target.operations->bind(target.context, id, output);
  }

  static auto data(void* context) -> perimortem_view_bytes {
    const auto target = Data::cast<Fixed>(context)->target;
    return target.operations->get_data(target.context);
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    const auto target = Data::cast<Fixed>(context)->target;
    return target.operations->resolve_concept(target.context, route);
  }

  static auto visit(void* context, ttx_concept_visitor callback) -> void {
    const auto target = Data::cast<Fixed>(context)->target;
    target.operations->visit_concepts(target.context, callback);
  }

  static constexpr ttx_abstract_ops operations = {supports, bind,   data,
                                                  resolve,  lookup, visit};
};

VALIDATION_TEST(Constants, explicit_constant_policy) {
  Fixed rejected{ttx_none()};
  const Abstract permanent(Fixed::resolve(&rejected));
  permanent.bind<Policies::None>().visit(
      [&](Policies::None answer) {
        EXPECT(
            answer.get_abstract().supports<Policies::Constant>() ==
            Negotiation::Status::Satisfied);
        EXPECT(answer.get_abstract() == permanent);
      },

      [&](Negotiation::Status) { EXPECT(False); });
  permanent.bind<Policies::Constant>().visit(
      [](Policies::Constant) {}, [&](Negotiation::Status) { EXPECT(False); });
  EXPECT(permanent.resolve() == permanent);

  Changing provider{ttx_unknown()};
  Fixed fixed{Changing::resolve(&provider)};
  Changing parent{Fixed::resolve(&fixed)};
  const Abstract root(Changing::resolve(&parent));
  const auto first = root.resolve_concept("selected"_view);
  EXPECT(
      first.supports<Policies::Constant>() == Negotiation::Status::Satisfied);
  EXPECT_EQ(first.get_data()[0], U8(17));
  EXPECT(first.resolve_concept("child"_view) == Abstract(ttx_unknown()));

  provider.value = 92;
  provider.child = ttx_none();

  const auto second = root.resolve_concept("selected"_view);
  EXPECT(first == second);
  EXPECT_EQ(second.get_data()[0], U8(92));

  const auto child = second.resolve_concept("child"_view);
  EXPECT(child == Abstract(ttx_none()));
  EXPECT(child.supports<Policies::Constant>() == Negotiation::Status::Unknown);
}

// The shared None result leaves the original provider free to select a
// different Abstract on its next observation.
VALIDATION_TEST(Constants, rejection_can_change) {
  Changing provider{ttx_none()};
  const Abstract subject(Changing::resolve(&provider));
  EXPECT(subject.resolve_concept("child"_view) == Abstract(ttx_none()));

  provider.child = Changing::resolve(&provider);
  EXPECT(subject.resolve_concept("child"_view) == subject);
}
