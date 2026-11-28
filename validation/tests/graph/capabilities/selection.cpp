// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/capabilities/selection.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/graph/policies/none.h"
#include "ttx/graph/policies/ordered.hpp"
#include "validation/tests/graph/fixtures/selection.h"
#include "validation/tests/graph/fixtures/value.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;

static Harness SelectionTests = {.name = "TTX::Selection"};

class Request {
 public:
  // The numeric value deliberately selects one fixture query interpretation.
  // Explicit construction prevents test counters from becoming requests.
  explicit Request(U8 value) : value(value) {}

  auto get_abstract() -> Abstract { return Abstract(this, operations); }
  auto set(U8 replacement) -> void { value = replacement; }

 private:
  static auto read(void* context) -> U8 {
    return Data::cast<Request>(context)->value;
  }

  static auto supports(void* context, perimortem_uuid requested)
      -> ttx_negotiation_status {
    const Identity::Uuid contract(requested);
    (void)context;

    if (contract == Validation::Value::contract_id ||
        contract == Abstract::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (!requested.high && requested.low <= 1) {
      return TTX_NEGOTIATION_REJECTED;
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid requested, void* output)
      -> ttx_negotiation_status {
    const auto status = supports(context, requested);
    if (status != TTX_NEGOTIATION_SATISFIED) {
      return status;
    }

    const Identity::Uuid contract(requested);
    if (contract == Validation::Value::contract_id) {
      *Data::cast<value_api>(output) = {context, read};
    } else {
      *Data::cast<ttx_abstract>(output) = {context, &operations};
    }

    return TTX_NEGOTIATION_SATISFIED;
  }

  static auto route(void* context, perimortem_view_bytes requested)
      -> ttx_abstract {
    (void)context;
    (void)requested;
    return ttx_none();
  }

  static auto visit(void* context, ttx_graph_visitor visitor) -> void {
    (void)context;
    (void)visitor;
  }

  static auto get_reachability(void* context) -> ttx_abstract_reachability {
    (void)context;
    return TTX_ABSTRACT_REACHABLE;
  }

  U8 value;
  inline static constexpr ttx_abstract_ops operations = {
    supports, bind, route, visit, get_reachability};
};

// The C provider evaluates each request before invoking the visitor. A private
// request without authority completes with zero callbacks, while the same
// request can expose its generated occurrence after authority changes.
VALIDATION_TEST(SelectionTests, provider_interprets_request) {
  selection_fixture provider = {
    0, TTX_NEGOTIATION_SATISFIED, TTX_NEGOTIATION_SATISFIED};

  const Abstract graph(selection_fixture_open(&provider));
  auto selection = graph.bind<Capabilities::Selection>();
  ASSERT(selection);

  Count ordinary = 0;
  auto visit_route = [&](View::Bytes, Abstract) { ++ordinary; };
  graph.visit_routes(Abstract::Visitor(visit_route));
  EXPECT_EQ(ordinary, Count(0));

  EXPECT(
      graph.resolve_route("first"_view).get_reachability() ==
      Abstract::Reachability::Unreachable);
  EXPECT(
      graph.resolve_route("second"_view).get_reachability() ==
      Abstract::Reachability::Unreachable);

  Request public_request(1);
  Request private_request(2);
  Request unsupported_request(3);
  Count first = 0;
  Count second = 0;
  auto observe = [&](View::Bytes route, Abstract value) {
    EXPECT(value.get_reachability() == Abstract::Reachability::Unreachable);
    if (first == 0 && second == 0) {
      public_request.set(2);
      provider.status = TTX_NEGOTIATION_REJECTED;
    }

    if (route == "first"_view) {
      ++first;
    } else if (route == "second"_view) {
      ++second;
    } else {
      EXPECT(False);
    }
  };

  selection.visit(
      [&](Capabilities::Selection selected) {
        EXPECT(
            selected.supports<Policies::Ordered>() ==
            Negotiation::Status::Satisfied);

        provider.ordering = TTX_NEGOTIATION_UNKNOWN;
        EXPECT(
            selected.supports<Policies::Ordered>() ==
            Negotiation::Status::Unknown);

        provider.ordering = TTX_NEGOTIATION_REJECTED;
        EXPECT(
            selected.supports<Policies::Ordered>() ==
            Negotiation::Status::Rejected);

        EXPECT(
            selected.select(
                public_request.get_abstract(), Abstract::Visitor(observe)) ==
            Negotiation::Status::Satisfied);
        EXPECT_EQ(first, Count(1));
        EXPECT_EQ(second, Count(1));
        EXPECT_EQ(provider.status, TTX_NEGOTIATION_REJECTED);
        public_request.set(1);
        provider.status = TTX_NEGOTIATION_SATISFIED;

        EXPECT(
            selected.select(
                private_request.get_abstract(), Abstract::Visitor(observe)) ==
            Negotiation::Status::Satisfied);
        EXPECT_EQ(first, Count(1));
        EXPECT_EQ(second, Count(1));

        provider.private_access = 1;
        EXPECT(
            selected.select(
                private_request.get_abstract(), Abstract::Visitor(observe)) ==
            Negotiation::Status::Satisfied);
        EXPECT_EQ(first, Count(2));
        EXPECT_EQ(second, Count(1));

        EXPECT(
            selected.select(
                unsupported_request.get_abstract(),
                Abstract::Visitor(observe)) == Negotiation::Status::Unknown);
        EXPECT_EQ(first, Count(2));
        EXPECT_EQ(second, Count(1));

        provider.status = TTX_NEGOTIATION_REJECTED;
        EXPECT(
            selected.select(
                public_request.get_abstract(), Abstract::Visitor(observe)) ==
            Negotiation::Status::Rejected);
        EXPECT_EQ(first, Count(2));
        EXPECT_EQ(second, Count(1));
      },

      [&] { EXPECT(False); });
}
