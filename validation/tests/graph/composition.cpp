// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/storage/vector.hpp"
#include "perimortem/core/view/null_terminated.hpp"
#include "perimortem/memory/allocator/arena.hpp"
#include "perimortem/memory/allocator/bibliotheca.hpp"

#include "ttx/graph/capabilities/retain.hpp"
#include "ttx/graph/policies/constant.hpp"
#include "validation/providers/composition/provider.h"
#include "validation/support/library.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Perimortem::Serialization;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

// Evaluation and graph reentry are independent supplied operations. These
// fixture contracts need only a UUID and their agreed C records.
class Expression {
 public:
  using Api = expression;
  static constexpr auto contract_id =
      Formats::Uuid(COMPOSITION_ID_HIGH, EXPRESSION_ID_LOW);

  // Explicit construction marks an unchecked conversion of the complete C
  // record. The provider supplies the evaluation and Abstract contracts.
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

class RetainedExpression {
 public:
  using Api = retained_expression;
  static constexpr auto contract_id =
      Formats::Uuid(COMPOSITION_ID_HIGH, RETAINED_EXPRESSION_ID_LOW);

  // Copies an acquired C record under its original release obligation.
  // Explicit construction marks this unchecked conversion from the C record.
  explicit RetainedExpression(Api api) : api(api) {}

  auto get_retained() const -> Policies::Retained {
    return Policies::Retained(api.operations->get_retained(api.context));
  }

  auto get_abstract() const -> Abstract {
    return get_retained().get_abstract();
  }

  auto evaluate() const -> U64 { return api.operations->evaluate(api.context); }

 private:
  Api api;
};

static Harness Composition = {.name = "TTX::Composition"};

VALIDATION_TEST(Composition, foreign_qualified_answer) {
  auto library = Validation::Support::open_library("libcomposition.so"_view);
  auto open = Data::cast<__remove_pointer(composition_open_function)>(
      Validation::Support::find_symbol(library, "composition_open"_view));
  auto acquire = Data::cast<__remove_pointer(composition_acquire_function)>(
      Validation::Support::find_symbol(library, "composition_acquire"_view));

  // Only the entry points are found by name. Negotiation and evaluation below
  // use hidden functions supplied in the returned tables, with the library
  // kept alive for every call and release.
  const Storage::Vector<View::Bytes, 2> hidden = {
    {"composition_bind"_view, "composition_evaluate"_view},
  };

  Allocator::Arena errors;
  for (Count name_index = 0; name_index < hidden.get_size(); ++name_index) {
    const auto name = hidden[name_index];
    library.symbol(name, errors)
        .visit([&](void*) { EXPECT(False); }, [](View::Bytes) {});
  }

  composition_state state = composition_state();
  state.value = 42;
  state.qualified = 1;
  state.additional = TTX_NEGOTIATION_UNKNOWN;

  const Abstract weak(open(&state));
  EXPECT(weak.supports<Expression>() == Negotiation::Status::Satisfied);
  EXPECT_EQ(state.acquisitions, U64(0));

  retained_expression acquired = retained_expression();
  ASSERT(acquire(&state, &acquired) == TTX_NEGOTIATION_SATISFIED);

  const RetainedExpression answer(acquired);
  const auto count = state.binds;
  EXPECT_EQ(answer.evaluate(), U64(42));
  EXPECT_EQ(state.binds, count);

  state.value = 99;

  const Formats::Uuid additional =
      Formats::Uuid(COMPOSITION_ID_HIGH, ADDITIONAL_ID_LOW);
  answer.get_abstract().bind<Expression>().visit(
      [&](Expression expression) {
        EXPECT(
            expression.get_abstract().supports<Policies::Retained>() ==
            Negotiation::Status::Satisfied);
        EXPECT(
            expression.get_abstract().supports(additional) ==
            Negotiation::Status::Unknown);
        state.additional = TTX_NEGOTIATION_REJECTED;
        EXPECT(
            expression.get_abstract().supports(additional) ==
            Negotiation::Status::Rejected);

        const auto binds = state.binds, visits = state.visits,
                   acquisitions = state.acquisitions;
        const Count requests = Allocator::Bibliotheca::check_out_requests();
        U64 sum = 0;
        for (Count index = 0; index != 1024; ++index) {
          sum += expression.evaluate();
        }

        EXPECT_EQ(sum, U64(42 * 1024));
        EXPECT_EQ(Allocator::Bibliotheca::check_out_requests(), requests);
        EXPECT_EQ(state.binds, binds);
        EXPECT_EQ(state.visits, visits);
        EXPECT_EQ(state.acquisitions, acquisitions);

        auto route = expression.get_abstract().resolve_route("self"_view);
        EXPECT(route.supports(additional) == Negotiation::Status::Rejected);
        route.bind<Policies::Constant>().visit(
            [&](Policies::Constant fixed) {
              EXPECT(
                  fixed.get_abstract().supports(additional) ==
                  Negotiation::Status::Rejected);
            },

            [&]() { EXPECT(False); });
      },

      [&]() { EXPECT(False); });

  U64 visited = 0;
  auto visit = [&](View::Bytes route, Abstract value) {
    EXPECT(route == "self"_view);
    EXPECT(value.supports(additional) == Negotiation::Status::Rejected);
    ++visited;
  };

  answer.get_abstract().visit_routes(Abstract::Visitor(visit));
  EXPECT_EQ(visited, U64(1));

  state.qualified = 0;
  EXPECT(
      answer.get_abstract().supports<Policies::Retained>() ==
      Negotiation::Status::Satisfied);
  EXPECT(
      answer.get_abstract().supports<Expression>() ==
      Negotiation::Status::Satisfied);
  answer.get_abstract().bind<RetainedExpression>().visit(
      [&](RetainedExpression) { EXPECT(False); }, [&] {});

  answer.get_abstract().bind<Capabilities::Retain>().visit(
      [&](Capabilities::Retain request) {
        request.retain().visit(
            [&](Policies::Retained second) {
              EXPECT_EQ(
                  second.get_abstract().get_abi().context,
                  answer.get_abstract().get_abi().context);
              answer.get_retained().release();
              EXPECT_EQ(state.references, U64(1));
              EXPECT(
                  second.get_abstract().supports<Expression>() ==
                  Negotiation::Status::Satisfied);
              second.release();
            },

            [&](Negotiation::Status) { EXPECT(False); });
      },

      [&]() { EXPECT(False); });
  EXPECT_EQ(state.references, U64(0));
  EXPECT_EQ(state.acquisitions, U64(2));
  EXPECT_EQ(state.releases, U64(2));
}

// Graph location survives binding through a typed context with a different
// layout. Typed reads identify the fixture state reached after each round
// trip. Context comparisons separately establish that the API records differ.
VALIDATION_TEST(Composition, substituted_round_trip) {
  auto library = Validation::Support::open_library("libcomposition.so"_view);
  auto open = Data::cast<__remove_pointer(composition_open_function)>(
      Validation::Support::find_symbol(library, "composition_open"_view));
  auto acquire = Data::cast<__remove_pointer(composition_acquire_function)>(
      Validation::Support::find_symbol(library, "composition_acquire"_view));

  composition_state state{};
  state.value = 42;
  state.qualified = 1;

  const Abstract discovery(open(&state));
  retained_expression acquired{};
  ASSERT_EQ(acquire(&state, &acquired), TTX_NEGOTIATION_SATISFIED);

  const RetainedExpression retained(acquired);
  const auto origin = retained.get_abstract();
  auto returned = origin;

  origin.bind<Capabilities::Retain>().visit(
      [&](Capabilities::Retain request) {
        EXPECT(request.get_abi().context != origin.get_abi().context);
        EXPECT(
            request.get_abstract().get_abi().context !=
            request.get_abi().context);
        request.get_abstract().bind<Expression>().visit(
            [&](Expression value) { EXPECT_EQ(value.evaluate(), U64(42)); },
            [&] { EXPECT(False); });
      },

      [&] { EXPECT(False); });

  for (Count index = 0; index < 4; ++index) {
    returned.bind<Expression>().visit(
        [&](Expression value) {
          EXPECT(value.get_abi().context != origin.get_abi().context);
          EXPECT_EQ(value.evaluate(), state.snapshot);
          returned = value.get_abstract();
          EXPECT(returned.get_abi().context != value.get_abi().context);
          EXPECT(returned.get_abi().context != origin.get_abi().context);
          EXPECT(returned.get_state() == Abstract::State::Present);

          returned.bind<Policies::Constant>().visit(
              [&](Policies::Constant policy) {
                returned = policy.get_abstract();
              },

              [&] { EXPECT(False); });

          // The fixture's other operation returns None, while get_abstract
          // reaches the retained expression. These two results exercise
          // arbitrary typed reentry and the explicit return promise.
          EXPECT(value.other().get_state() == Abstract::State::None);
        },

        [&] { EXPECT(False); });

    ++state.snapshot;
  }

  // Constant fixes selection, while the provider can change the typed data.
  // Later binding refusal leaves earlier supplied operations usable.
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

  EXPECT_EQ(state.acquisitions, U64(1));
  EXPECT_EQ(state.releases, U64(0));
  retained.get_retained().release();
  EXPECT_EQ(state.releases, U64(1));
  EXPECT_EQ(state.references, U64(0));
}
