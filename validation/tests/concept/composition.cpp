// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/storage/vector.hpp"
#include "perimortem/core/view/null_terminated.hpp"
#include "perimortem/memory/allocator/arena.hpp"
#include "perimortem/memory/allocator/bibliotheca.hpp"

#include "ttx/concept/capabilities/borrow.hpp"
#include "ttx/concept/policies/constant.hpp"
#include "validation/providers/composition/provider.h"
#include "validation/support/library.hpp"

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
      Serialization::Formats::Uuid(COMPOSITION_ID_HIGH, EXPRESSION_ID_LOW);
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
  static constexpr auto contract_id = Serialization::Formats::Uuid(
      COMPOSITION_ID_HIGH,
      BORROWED_EXPRESSION_ID_LOW);
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
  auto library = Validation::Support::open_library("libcomposition.so"_view);
  auto open = reinterpret_cast<composition_open_function>(
      Validation::Support::find_symbol(library, "composition_open"_view));
  auto acquire = reinterpret_cast<composition_acquire_function>(
      Validation::Support::find_symbol(library, "composition_acquire"_view));

  // Only the entry points are found by name. Negotiation and evaluation below
  // use hidden functions supplied in the returned tables, with the library
  // kept alive for every call and release.
  const Core::Storage::Vector<Core::View::Bytes, 2> hidden = {
    {"composition_bind"_view, "composition_evaluate"_view},
  };

  Memory::Allocator::Arena errors;
  for (Count name_index = 0; name_index < hidden.get_size(); ++name_index) {
    const auto name = hidden[name_index];
    library.symbol(name, errors)
        .visit([&](void*) { EXPECT(False); }, [](Core::View::Bytes) {});
  }

  composition_state state = composition_state();
  state.value = 42;
  state.qualified = 1;
  state.additional = TTX_BINDING_UNKNOWN;

  const composition_forms forms = composition_forms(
      &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
          Abstract::Api>::reference>::get_representation(),
      &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
          Capabilities::Borrow::Api>::reference>::get_representation(),
      &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
          Policies::Borrowed::Api>::reference>::get_representation(),
      &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
          Expression::Api>::reference>::get_representation(),
      &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
          BorrowedExpression::Api>::reference>::get_representation());
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

  const Serialization::Formats::Uuid additional =
      Serialization::Formats::Uuid(COMPOSITION_ID_HIGH, ADDITIONAL_ID_LOW);
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
        const Count requests =
            Memory::Allocator::Bibliotheca::check_out_requests();
        U64 sum = 0;
        for (Count index = 0; index != 1024; ++index) {
          sum += expression.evaluate();
        }

        EXPECT_EQ(sum, U64(42 * 1024));
        EXPECT_EQ(
            Memory::Allocator::Bibliotheca::check_out_requests(), requests);
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

            [&](Binding::Status) { EXPECT(False); });
      },

      [&](Binding::Status) { EXPECT(False); });

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
      [&](Binding::Status failure) {
        EXPECT(failure == Binding::Status::Rejected);
      });

  answer.get_abstract().bind<Capabilities::Borrow>().visit(
      [&](Capabilities::Borrow request) {
        request.borrow().visit(
            [&](Policies::Borrowed second) {
              EXPECT_EQ(
                  second.get_abstract().get_abi().context,
                  answer.get_abstract().get_abi().context);
              answer.get_borrowed().release();
              EXPECT_EQ(state.references, U64(1));
              EXPECT(
                  second.get_abstract().supports<Expression>() ==
                  Binding::Status::Satisfied);
              second.release();
            },

            [&](Binding::Status) { EXPECT(False); });
      },

      [&](Binding::Status) { EXPECT(False); });
  EXPECT_EQ(state.references, U64(0));
  EXPECT_EQ(state.acquisitions, U64(2));
  EXPECT_EQ(state.releases, U64(2));
}
