// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/constant.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.hpp"
#include "validation/tests/graph/fixtures/value.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Serialization;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness Constants = {.name = "TTX::Constant"};

// This provider exposes one changing outgoing edge. The consumer keeps the
// provider's identity while asking it again for each child observation.
struct Changing {
  ttx_abstract child;
  U8 value = 17;

  static auto state(void* context) -> ttx_abstract_state {
    return TTX_ABSTRACT_PRESENT;
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (Formats::Uuid(id) == Validation::Value::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    return Formats::Uuid(id) == Abstract::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    if (Formats::Uuid(id) == Validation::Value::contract_id) {
      *Data::cast<value_api>(output) = {context, read};
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (supports(context, id) != TTX_NEGOTIATION_SATISFIED) {
      return TTX_NEGOTIATION_UNKNOWN;
    }

    Abstract::Api api = get_abstract(context);
    *Data::cast<Abstract::Api>(output) = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  static auto read(void* context) -> U8 {
    return Data::cast<Changing>(context)->value;
  }

  static auto get_abstract(void* context) -> ttx_abstract {
    return {context, &operations};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    return Data::cast<Changing>(context)->child;
  }

  static auto visit(void* context, ttx_graph_visitor callback) -> void {}

  static constexpr ttx_abstract_ops operations = {
    supports, bind, lookup, visit, state};
};

// Fixed preserves the selected Abstract. Outgoing routes still use the
// target's operations and carry their own policies independently of Constant.
struct Fixed {
  ttx_abstract target;

  static auto state(void* context) -> ttx_abstract_state {
    const auto target = Data::cast<Fixed>(context)->target;
    return target.operations->get_state(target.context);
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (Formats::Uuid(id) == Policies::Constant::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    const auto target = Data::cast<Fixed>(context)->target;
    return target.operations->supports(target.context, id);
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    const Formats::Uuid contract(id);
    if (contract == Abstract::contract_id ||
        contract == Policies::Constant::contract_id ||
        ((contract == Policies::None::contract_id ||
          contract == Policies::Unknown::contract_id) &&
         supports(context, id) == TTX_NEGOTIATION_SATISFIED)) {
      Abstract::Api api = get_abstract(context);
      *Data::cast<Abstract::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    const auto target = Data::cast<Fixed>(context)->target;
    return target.operations->bind(target.context, id, output);
  }

  static auto get_abstract(void* context) -> ttx_abstract {
    return {context, &operations};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    const auto target = Data::cast<Fixed>(context)->target;
    return target.operations->resolve_route(target.context, route);
  }

  static auto visit(void* context, ttx_graph_visitor callback) -> void {
    const auto target = Data::cast<Fixed>(context)->target;
    target.operations->visit_routes(target.context, callback);
  }

  static constexpr ttx_abstract_ops operations = {
    supports, bind, lookup, visit, state};
};

VALIDATION_TEST(Constants, explicit_constant_policy) {
  Fixed rejected{ttx_none()};
  const Abstract permanent(Fixed::get_abstract(&rejected));
  EXPECT(permanent.get_state() == Abstract::State::None);
  permanent.bind<Policies::None>().visit(
      [&](Policies::None answer) {
        EXPECT(
            answer.get_abstract().supports<Policies::Constant>() ==
            Negotiation::Status::Satisfied);
        EXPECT(answer.get_abstract() == permanent);
      },

      [&]() { EXPECT(False); });
  permanent.bind<Policies::Constant>().visit(
      [](Policies::Constant) {}, [&]() { EXPECT(False); });

  Changing provider{ttx_unknown()};
  Fixed fixed{Changing::get_abstract(&provider)};
  Changing parent{Fixed::get_abstract(&fixed)};
  const Abstract root(Changing::get_abstract(&parent));
  const auto first = root.resolve_route("selected"_view);
  EXPECT(first.get_state() == Abstract::State::Present);
  EXPECT(
      first.supports<Policies::Constant>() == Negotiation::Status::Satisfied);
  EXPECT_EQ(
      first.bind<Validation::Value>().visit(
          [](Validation::Value value) { return value.read(); },
          [&] {
            EXPECT(False);
            return U8(0);
          }),

      U8(17));
  EXPECT(first.resolve_route("child"_view) == Abstract(ttx_unknown()));

  provider.value = 92;
  provider.child = ttx_none();

  const auto second = root.resolve_route("selected"_view);
  EXPECT(first == second);
  EXPECT_EQ(
      second.bind<Validation::Value>().visit(
          [](Validation::Value value) { return value.read(); },
          [&] {
            EXPECT(False);
            return U8(0);
          }),

      U8(92));

  const auto child = second.resolve_route("child"_view);
  EXPECT(child == Abstract(ttx_none()));
  EXPECT(child.supports<Policies::Constant>() == Negotiation::Status::Unknown);
}

// The shared None result leaves the original provider free to select a
// different Abstract on its next observation.
VALIDATION_TEST(Constants, rejection_can_change) {
  Changing provider{ttx_none()};
  const Abstract subject(Changing::get_abstract(&provider));
  EXPECT(subject.resolve_route("child"_view) == Abstract(ttx_none()));

  provider.child = Changing::get_abstract(&provider);
  EXPECT(subject.resolve_route("child"_view) == subject);
}
