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
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness Constants = {.name = "TTX::Constant"};

// The "missing" route has no target for the lifetime of this provider.
// Its other routes select the current child, which may become available later.
struct Changing {
  ttx_abstract child;
  U8 value = 17;

  static auto get_reachability(void* context) -> ttx_abstract_reachability {
    return TTX_ABSTRACT_REACHABLE;
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (Identity::Uuid(id) == Validation::Value::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    return Identity::Uuid(id) == Abstract::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    if (Identity::Uuid(id) == Validation::Value::contract_id) {
      *Data::cast<value_api>(output) = {context, read};
      return TTX_NEGOTIATION_SATISFIED;
    }

    const auto status = supports(context, id);
    if (status != TTX_NEGOTIATION_SATISFIED) {
      return status;
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
    if (View::Bytes(route) == "missing"_view) {
      return ttx_none();
    }

    return Data::cast<Changing>(context)->child;
  }

  static auto visit(void* context, ttx_graph_visitor callback) -> void {}

  static constexpr ttx_abstract_ops operations = {
    supports, bind, lookup, visit, get_reachability};
};

// Fixed preserves the selected Abstract. Outgoing routes still use the
// target's operations and carry their own policies independently of Constant.
struct Fixed {
  ttx_abstract target;

  static auto get_reachability(void* context) -> ttx_abstract_reachability {
    const auto target = Data::cast<Fixed>(context)->target;
    return target.operations->get_reachability(target.context);
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (Identity::Uuid(id) == Policies::Unknown::contract_id) {
      return TTX_NEGOTIATION_REJECTED;
    }

    if (Identity::Uuid(id) == Policies::Constant::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    const auto target = Data::cast<Fixed>(context)->target;
    return target.operations->supports(target.context, id);
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    const Identity::Uuid contract(id);
    if (contract == Policies::Unknown::contract_id) {
      return TTX_NEGOTIATION_REJECTED;
    }

    const auto target = Data::cast<Fixed>(context)->target;
    if (contract == Abstract::contract_id ||
        contract == Policies::Constant::contract_id) {
      Abstract::Api api = get_abstract(context);
      *Data::cast<Abstract::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    return target.operations->bind(target.context, id, output);
  }

  static auto get_abstract(void* context) -> ttx_abstract {
    return {context, &operations};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    const auto target = Data::cast<Fixed>(context)->target;
    if (Abstract(target).supports<Policies::None>() ==
        Negotiation::Status::Satisfied) {
      return get_abstract(context);
    }

    return target.operations->resolve_route(target.context, route);
  }

  static auto visit(void* context, ttx_graph_visitor callback) -> void {
    const auto target = Data::cast<Fixed>(context)->target;
    target.operations->visit_routes(target.context, callback);
  }

  static constexpr ttx_abstract_ops operations = {
    supports, bind, lookup, visit, get_reachability};
};

// Adding Constant gives the caller a guarantee the Unknown policy leaves
// unsettled. The wrapper still reports the reachability of its target.
VALIDATION_TEST(Constants, constant_indeterminate_target) {
  Fixed fixed{ttx_unknown()};
  const Abstract subject(Fixed::get_abstract(&fixed));
  EXPECT(subject.get_reachability() == Abstract::Reachability::Indeterminate);
  EXPECT(
      subject.supports<Policies::Constant>() == Negotiation::Status::Satisfied);
  EXPECT(
      subject.supports<Policies::Unknown>() == Negotiation::Status::Rejected);

  const ttx_abstract api = subject;
  ttx_abstract output{};
  EXPECT_EQ(
      api.operations->bind(
          api.context, Policies::Unknown::contract_id, &output),
      TTX_NEGOTIATION_REJECTED);
  ASSERT_EQ(
      api.operations->bind(
          api.context, Policies::Constant::contract_id, &output),
      TTX_NEGOTIATION_SATISFIED);
  EXPECT(
      Abstract(output).get_reachability() ==
      Abstract::Reachability::Indeterminate);
}

VALIDATION_TEST(Constants, explicit_constant_policy) {
  Fixed stable_absence{ttx_none()};
  const Abstract absent(Fixed::get_abstract(&stable_absence));
  EXPECT(absent.get_reachability() == Abstract::Reachability::Unreachable);
  EXPECT(absent != Abstract(ttx_none()));
  EXPECT(absent.supports<Policies::None>() == Negotiation::Status::Satisfied);
  EXPECT(
      absent.supports<Policies::Constant>() == Negotiation::Status::Satisfied);
  EXPECT_NOT(absent.bind<Policies::None>());
  EXPECT(absent.bind<Policies::Constant>());
  absent.bind<Abstract>().visit(
      [&](Abstract answer) {
        EXPECT(answer == absent);
        EXPECT(
            answer.get_reachability() == Abstract::Reachability::Unreachable);
        EXPECT(
            answer.resolve_route("child"_view).get_reachability() ==
            Abstract::Reachability::Unreachable);
      },

      [&]() { EXPECT(False); });

  Changing provider{ttx_unknown()};
  Fixed fixed{Changing::get_abstract(&provider)};
  Fixed equivalent{Changing::get_abstract(&provider)};
  Changing parent{Fixed::get_abstract(&fixed)};
  const Abstract root(Changing::get_abstract(&parent));
  const auto first = root.resolve_route("selected"_view);
  EXPECT(first.get_reachability() == Abstract::Reachability::Reachable);
  EXPECT(
      first.supports<Policies::Constant>() == Negotiation::Status::Satisfied);
  first.bind<Policies::Constant>().visit(
      [&](Policies::Constant policy) {
        EXPECT(ttx_abstract(policy.get_abstract()).context == &fixed);
      },

      [&] { EXPECT(False); });
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
  parent.child = Fixed::get_abstract(&equivalent);

  const auto second = root.resolve_route("selected"_view);
  EXPECT(ttx_abstract(first).context != ttx_abstract(second).context);
  EXPECT_EQ(
      first.bind<Validation::Value>().visit(
          [](Validation::Value value) { return value.read(); },
          [&] {
            EXPECT(False);
            return U8(0);
          }),

      U8(92));
  EXPECT_EQ(
      second.bind<Validation::Value>().visit(
          [](Validation::Value value) { return value.read(); },
          [&] {
            EXPECT(False);
            return U8(0);
          }),

      U8(92));

  const auto child = second.resolve_route("child"_view);
  EXPECT(first.resolve_route("child"_view) == child);
  EXPECT(child.get_reachability() == Abstract::Reachability::Unreachable);
  EXPECT(child.supports<Policies::Constant>() == Negotiation::Status::Rejected);
}

// A caller holding the result of a missing lookup can still inspect it after
// the same route begins selecting a child.
VALIDATION_TEST(Constants, absence_is_one_observation) {
  Changing provider{ttx_none()};
  const Abstract origin(Changing::get_abstract(&provider));
  const auto missing = origin.resolve_route("child"_view);
  ASSERT(missing.supports<Policies::None>() == Negotiation::Status::Satisfied);
  EXPECT(
      missing.supports<Policies::Constant>() == Negotiation::Status::Rejected);

  provider.child = origin;
  EXPECT(origin.resolve_route("child"_view) == origin);
  EXPECT(missing.get_reachability() == Abstract::Reachability::Unreachable);
}

VALIDATION_TEST(Constants, present_bindings) {
  Changing changing{ttx_none()};
  Fixed fixed{Changing::get_abstract(&changing)};
  const ttx_abstract graphs[] = {
    Changing::get_abstract(&changing),
    Fixed::get_abstract(&fixed),
  };

  const perimortem_uuid ids[] = {
    {TTX_NONE_ID_HIGH, TTX_NONE_ID_LOW},
    {TTX_UNKNOWN_ID_HIGH, TTX_UNKNOWN_ID_LOW},
  };

  for (const auto api : graphs) {
    EXPECT_EQ(
        api.operations->get_reachability(api.context), TTX_ABSTRACT_REACHABLE);
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
