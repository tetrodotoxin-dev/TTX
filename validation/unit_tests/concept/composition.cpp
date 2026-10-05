// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/support/library.hpp"
#include "validation/support/measurement.hpp"

#include "perimortem/core/static/vector.hpp"
#include "perimortem/core/null_terminated.hpp"

#include "perimortem/memory/allocator/arena.hpp"

#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/capabilities/borrow.hpp"
#include "ttx/concept/policies/constant.hpp"
#include "validation/providers/composition/provider.h"

using namespace Perimortem;
using namespace Ttx;
using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

TTX_DATA_RECORD(
    expression_ops,
    TTX_DATA_MEMBER(expression_ops, evaluate),
    TTX_DATA_MEMBER(expression_ops, abstract));
TTX_DATA_RECORD(
    expression,
    TTX_DATA_MEMBER(expression, context),
    TTX_DATA_MEMBER(expression, operations));
TTX_DATA_RECORD(
    borrowed_expression_ops,
    TTX_DATA_MEMBER(borrowed_expression_ops, evaluate),
    TTX_DATA_MEMBER(borrowed_expression_ops, borrowed));
TTX_DATA_RECORD(
    borrowed_expression,
    TTX_DATA_MEMBER(borrowed_expression, context),
    TTX_DATA_MEMBER(borrowed_expression, operations));

// These contracts place evaluation before the embedded Abstract table. Each
// consumer keeps the complete negotiated record and selects that member
// explicitly.
class Expression {
 public:
  using Api = expression;
  static constexpr auto contract_id =
      System::Uuid(COMPOSITION_ID_HIGH, EXPRESSION_ID_LOW);
  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->evaluate &&
           Abstract::accept({api.context, &api.operations->abstract});
  }

  explicit Expression(Api api) : api(api) {}

  auto get_abstract() const -> Abstract {
    return Abstract(api.context, api.operations->abstract);
  }

  auto evaluate() const -> U64 { return api.operations->evaluate(api.context); }

 private:
  Api api;
};
class BorrowedExpression {
 public:
  using Api = borrowed_expression;
  static constexpr auto contract_id =
      System::Uuid(COMPOSITION_ID_HIGH, BORROWED_EXPRESSION_ID_LOW);
  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->evaluate &&
           Policies::Borrowed::accept({api.context, &api.operations->borrowed});
  }

  explicit BorrowedExpression(Api api) : api(api) {}

  auto get_borrowed() const -> Policies::Borrowed {
    return Policies::Borrowed({api.context, &api.operations->borrowed});
  }

  auto get_abstract() const -> Abstract {
    return get_borrowed().get_abstract();
  }

  auto evaluate() const -> U64 { return api.operations->evaluate(api.context); }

 private:
  Api api;
};
static Toolchain::Validation::Harness Composition = {
  .name = "TTX::Composition"};

VALIDATION_TEST(Composition, foreign_qualified_answer) {
  auto library = Validation::open_library("libcomposition.so"_view);
  auto open = reinterpret_cast<composition_open_function>(
      Validation::find_symbol(library, "composition_open"_view));
  auto acquire = reinterpret_cast<composition_acquire_function>(
      Validation::find_symbol(library, "composition_acquire"_view));

  // Only the entry points are found by name. Negotiation and evaluation below
  // use hidden functions supplied in the returned tables, with the library
  // kept alive for every call and release.
  const Core::Static::Vector<Core::View::Bytes, 2> hidden = {
    {"composition_bind"_view, "composition_evaluate"_view},
  };
  Memory::Allocator::Arena errors;
  for (const auto name : hidden.get_view()) {
    library.symbol(name, errors)
        .visit([&](void*) { EXPECT(False); }, [](Core::View::Bytes) {});
  }

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
  answer.get_abstract().bind<Expression>().visit(
      [&](Expression expression) {
        EXPECT(
            expression.get_abstract().supports<Policies::Borrowed>() ==
            Binding::Status::Satisfied);
        EXPECT(
            expression.get_abstract().supports(additional) ==
            Binding::Status::Unknown);
        state.additional = TTX_BINDING_REJECTED;
        EXPECT(
            expression.get_abstract().supports(additional) ==
            Binding::Status::Rejected);
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
        auto route = expression.get_abstract().resolve_concept("self"_view);
        EXPECT(route.supports(additional) == Binding::Status::Rejected);
        route.bind<Policies::Constant>().visit(
            [&](Policies::Constant fixed) {
              EXPECT(
                  fixed.get_abstract().supports(additional) ==
                  Binding::Status::Rejected);
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
  answer.get_abstract().visit_concepts(Abstract::Visitor(visit));
  EXPECT_EQ(visited, U64(1));

  state.qualified = 0;
  EXPECT(
      answer.get_abstract().supports<Policies::Borrowed>() ==
      Binding::Status::Satisfied);
  EXPECT(
      answer.get_abstract().supports<Expression>() ==
      Binding::Status::Satisfied);
  answer.get_abstract().bind<BorrowedExpression>().visit(
      [&](BorrowedExpression) { EXPECT(False); },
      [&](Binding::Failure failure) {
        EXPECT(failure == Binding::Failure::Rejected);
      });
  answer.get_abstract().bind<Capabilities::Borrow>().visit(
      [&](Capabilities::Borrow request) {
        request.borrow().visit(
            [&](Policies::Borrowed second) {
              EXPECT_EQ(
                  second.get_abstract().get_identity(),
                  answer.get_abstract().get_identity());
              answer.get_borrowed().release();
              EXPECT_EQ(state.references, U64(1));
              EXPECT(
                  second.get_abstract().supports<Expression>() ==
                  Binding::Status::Satisfied);
              second.release();
            },
            [&](Binding::Failure) { EXPECT(False); });
      },
      [&](Binding::Failure) { EXPECT(False); });
  EXPECT_EQ(state.references, U64(0));
  EXPECT_EQ(state.acquisitions, U64(2));
  EXPECT_EQ(state.releases, U64(2));
}
