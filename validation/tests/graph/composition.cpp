// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#ifndef TOOLCHAIN_WASM

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/graph/policies/constant.hpp"
#include "validation/providers/composition/provider.h"
#include "validation/support/library.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;

class Expression {
 public:
  using Api = expression;
  static constexpr auto contract_id =
      Identity::Uuid(COMPOSITION_ID_HIGH, EXPRESSION_ID_LOW);

  // Construction accepts only a record already supplied under this fixture's
  // UUID. Explicit construction keeps that ABI interpretation at the call site.
  explicit Expression(Api api) : api(api) {}
  auto get_abi() const -> Api { return api; }

  auto get_abstract() const -> Abstract {
    return Abstract(api.operations->get_abstract(api.context));
  }

  auto evaluate() const -> U64 { return api.operations->evaluate(api.context); }

  auto other() const -> Abstract {
    return Abstract(api.operations->other(api.context));
  }

 private:
  Api api;
};

static Harness Composition = {.name = "TTX::Composition"};

// Graph location survives binding through a typed context with a different
// layout. Typed reads identify the fixture state reached after each round
// trip. Context comparisons separately establish that the API records differ.
VALIDATION_TEST(Composition, substituted_round_trip) {
  auto library = Validation::Support::open_library("libcomposition.so"_view);
  auto open = Data::cast<__remove_pointer(composition_open_function)>(
      Validation::Support::find_symbol(library, "composition_open"_view));

  composition_state state{};
  state.value = 42;
  state.additional = TTX_NEGOTIATION_UNKNOWN;

  const Abstract origin(open(&state));
  auto returned = origin;
  const Identity::Uuid additional =
      Identity::Uuid(COMPOSITION_ID_HIGH, ADDITIONAL_ID_LOW);

  for (Count index = 0; index < 4; ++index) {
    returned.bind<Expression>().visit(
        [&](Expression value) {
          EXPECT(value.get_abi().context != returned.get_abi().context);
          EXPECT_EQ(value.evaluate(), state.value);

          returned = value.get_abstract();
          EXPECT(returned.get_abi().context != value.get_abi().context);
          EXPECT(returned.get_abi().context != origin.get_abi().context);
          EXPECT(returned.get_state() == Abstract::State::Present);

          returned.bind<Policies::Constant>().visit(
              [&](Policies::Constant policy) {
                returned = policy.get_abstract();
              },

              [&] { EXPECT(False); });

          EXPECT(value.other().get_state() == Abstract::State::None);
        },

        [&] { EXPECT(False); });

    ++state.value;
  }

  Count visits = 0;
  auto visit = [&](View::Bytes route, Abstract value) {
    EXPECT_EQ(route, "self"_view);
    EXPECT(value.supports(additional) == Negotiation::Status::Unknown);
    ++visits;
  };

  returned.visit_routes(Abstract::Visitor(visit));
  EXPECT_EQ(visits, Count(1));

  returned.bind<Expression>().visit(
      [&](Expression value) {
        EXPECT_EQ(value.evaluate(), U64(46));
        state.refuse_bindings = 1;
        EXPECT_NOT(value.get_abstract().bind<Expression>());
        EXPECT_NOT(value.get_abstract().bind<Abstract>());
        EXPECT(value.get_abstract().get_state() == Abstract::State::Present);
        EXPECT_EQ(value.evaluate(), U64(46));
      },

      [&] { EXPECT(False); });
}

#endif
