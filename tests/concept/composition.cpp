// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tests/concept/fixtures/composition.h"

#include "perimortem/core/null_terminated.hpp"

#include "tests/library.hpp"
#include "tests/semantic/measurement.hpp"
#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/capabilities/borrow.hpp"
#include "ttx/concept/policies/constant.hpp"

using namespace Perimortem;
using namespace Ttx;
using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

TTX_DATA_RECORD(
    expression_ops,
    TTX_DATA_MEMBER(expression_ops, abstract),
    TTX_DATA_MEMBER(expression_ops, evaluate));
TTX_DATA_RECORD(
    expression,
    TTX_DATA_MEMBER(expression, source),
    TTX_DATA_MEMBER(expression, operations));
TTX_DATA_RECORD(
    borrowed_expression_ops,
    TTX_DATA_MEMBER(borrowed_expression_ops, borrowed),
    TTX_DATA_MEMBER(borrowed_expression_ops, evaluate));
TTX_DATA_RECORD(
    borrowed_expression,
    TTX_DATA_MEMBER(borrowed_expression, source),
    TTX_DATA_MEMBER(borrowed_expression, operations));

class Expression : public Abstract {
 public:
  using Api = expression;
  static constexpr auto contract_id =
      System::Uuid(COMPOSITION_ID_HIGH, EXPRESSION_ID_LOW);
  explicit Expression(Api api)
      : Abstract(api.source, api.operations->abstract) {}
  auto evaluate() const -> U64 {
    const auto api = get_abi();
    return reinterpret_cast<const expression_ops*>(api.operations)
        ->evaluate(api.source);
  }
};
class BorrowedExpression : public Policies::Borrowed {
 public:
  using Api = borrowed_expression;
  static constexpr auto contract_id =
      System::Uuid(COMPOSITION_ID_HIGH, BORROWED_EXPRESSION_ID_LOW);
  static auto accept(Api api) -> Bool {
    return api.operations &&
           Policies::Borrowed::accept(
               ttx_borrowed(api.source, &api.operations->borrowed)) &&
           api.operations->evaluate;
  }
  explicit BorrowedExpression(Api api)
      : Borrowed(ttx_borrowed(api.source, &api.operations->borrowed)) {}
  auto evaluate() const -> U64 {
    const auto api = Abstract::get_abi();
    return reinterpret_cast<const borrowed_expression_ops*>(api.operations)
        ->evaluate(api.source);
  }
};
static Toolchain::Validation::Harness Composition = {
  .name = "TTX::Composition"};

VALIDATION_TEST(Composition, foreign_qualified_answer) {
  auto library = Validation::open_library("libcomposition.so"_view);
  auto open = reinterpret_cast<composition_open_function>(
      Validation::find_symbol(library, "composition_open"_view));
  auto acquire = reinterpret_cast<composition_acquire_function>(
      Validation::find_symbol(library, "composition_acquire"_view));
  composition_state state = composition_state();
  state.value = 42;
  state.qualified = 1;
  state.additional = TTX_BINDING_UNKNOWN;
  const composition_forms forms = composition_forms(
      &Binding::representation<Abstract>(),
      &Binding::representation<Capabilities::Borrow>(),
      &Binding::representation<Policies::Borrowed>(),
      &Binding::representation<Expression>(),
      &Binding::representation<BorrowedExpression>());
  const Abstract weak(open(&state, forms));
  EXPECT(weak.supports<Expression>() == Binding::Status::Satisfied);
  EXPECT_EQ(state.acquisitions, U64(0));

  borrowed_expression acquired = borrowed_expression();
  ASSERT(acquire(&state, &acquired) == TTX_BINDING_SATISFIED);
  const BorrowedExpression answer(acquired);
  const auto count = state.binds;
  EXPECT_EQ(answer.evaluate(), U64(42));
  EXPECT_EQ(state.binds, count);
  state.value = 99;
  const System::Uuid additional =
      System::Uuid(COMPOSITION_ID_HIGH, ADDITIONAL_ID_LOW);
  answer.bind<Expression>().visit(
      [&](Expression expression) {
        EXPECT(
            expression.supports<Policies::Borrowed>() ==
            Binding::Status::Satisfied);
        EXPECT(expression.supports(additional) == Binding::Status::Unknown);
        state.additional = TTX_BINDING_REJECTED;
        EXPECT(expression.supports(additional) == Binding::Status::Rejected);
        const auto binds = state.binds, visits = state.visits,
                   acquisitions = state.acquisitions;
        Validation::FlowTests::Measurement measurement;
        U64 sum = 0;
        for (Count index = 0; index != 1024; ++index) {
          sum += expression.evaluate();
        }
        measurement.stop();
        EXPECT_EQ(sum, U64(42 * 1024));
        EXPECT_EQ(measurement.get_allocations(), Count(0));
        EXPECT_EQ(state.binds, binds);
        EXPECT_EQ(state.visits, visits);
        EXPECT_EQ(state.acquisitions, acquisitions);
        auto route = expression.resolve_concept("self"_view);
        EXPECT(route.supports(additional) == Binding::Status::Rejected);
        route.bind<Policies::Constant>().visit(
            [&](Policies::Constant fixed) {
              EXPECT(fixed.supports(additional) == Binding::Status::Rejected);
            },
            [&](Binding::Failure) { EXPECT(False); });
      },
      [&](Binding::Failure) { EXPECT(False); });

  U64 visited = 0;
  auto visit = [&](Core::View::Bytes route, Abstract value) {
    EXPECT(route == "self"_view);
    EXPECT(value.supports(additional) == Binding::Status::Rejected);
    ++visited;
  };
  answer.visit_concepts(Abstract::Visitor(visit));
  EXPECT_EQ(visited, U64(1));
  state.qualified = 0;
  EXPECT(answer.supports<Policies::Borrowed>() == Binding::Status::Satisfied);
  EXPECT(answer.supports<Expression>() == Binding::Status::Satisfied);
  answer.bind<BorrowedExpression>().visit(
      [&](BorrowedExpression) { EXPECT(False); },
      [&](Binding::Failure failure) {
        EXPECT(failure == Binding::Failure::Rejected);
      });
  answer.bind<Capabilities::Borrow>().visit(
      [&](Capabilities::Borrow request) {
        request.borrow().visit(
            [&](Policies::Borrowed second) {
              EXPECT_EQ(second.get_identity(), answer.get_identity());
              answer.release();
              EXPECT_EQ(state.references, U64(1));
              EXPECT(
                  second.supports<Expression>() == Binding::Status::Satisfied);
              second.release();
            },
            [&](Binding::Failure) { EXPECT(False); });
      },
      [&](Binding::Failure) { EXPECT(False); });
  EXPECT_EQ(state.references, U64(0));
  EXPECT_EQ(state.acquisitions, U64(2));
  EXPECT_EQ(state.releases, U64(2));
}
