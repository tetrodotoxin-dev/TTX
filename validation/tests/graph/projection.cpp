// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/projection.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/identity/uuid.hpp"
#include "perimortem/core/view/vector.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;

class Signal {
 public:
  using Api = U8;
  static constexpr auto contract_id =
      Identity::Uuid(0xb2f403c92a714921ULL, 0xa3055724f5f4166aULL);

  explicit constexpr Signal(Api api) { (void)api; }
};

class BoundValue {
 public:
  struct Api {
    U64 value;
  };

  static constexpr auto contract_id =
      Identity::Uuid(0xe76527d355d241f5ULL, 0x97ce99ad635776b1ULL);

  explicit constexpr BoundValue(Api api) : api(api) {}

  constexpr auto get_value() const -> U64 { return api.value; }

 private:
  Api api;
};

class Value
    : public Projection<Value, 0xb4a264ce6a714f90ULL, 0xa18c4321ea5c6e9cULL> {
 public:
  inline static constexpr Identity::Uuid signal = Signal::contract_id;
  inline static constexpr View::Vector<Identity::Uuid> supported_contracts =
      View::Vector<Identity::Uuid>(signal, 1);

  static constexpr auto create(U64 value) -> Value { return Value(value); }

  constexpr auto get_value() const -> U64 { return value; }

  auto bind_contract(Identity::Uuid contract, void* output) const
      -> Negotiation::Status {
    if (contract != BoundValue::contract_id) {
      return Negotiation::Status::Unknown;
    }

    BoundValue::Api api;
    api.value = value;
    *Data::cast<BoundValue::Api>(output) = api;
    return Negotiation::Status::Satisfied;
  }

 private:
  explicit constexpr Value(U64 value) : value(value) {}

  U64 value;
};

// This provider has a graph context unrelated to its typed context. It
// materializes a Value when the typed operation is called and returns to the
// provider's original graph record through its own get_abstract operation.
class Simulacrum {
  struct GraphContext {
    Simulacrum* owner = nullptr;
  };

  struct TypedContext {
    Simulacrum* owner = nullptr;
    const void* value = nullptr;
  };

 public:
  auto initialize(const void* value) -> void {
    graph.owner = this;
    typed.owner = this;
    typed.value = value;
  }

  auto get_abstract() -> Abstract { return Abstract(&graph, graph_operations); }

 private:
  static auto supports(void* context, perimortem_uuid requested)
      -> ttx_negotiation_status {
    (void)context;

    const Identity::Uuid contract(requested);
    if (!requested.high && requested.low <= 1) {
      return TTX_NEGOTIATION_REJECTED;
    }

    return contract == Value::Abstract::contract_id ||
                   contract == Abstract::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid requested, void* output)
      -> ttx_negotiation_status {
    auto& provider = *Data::cast<GraphContext>(context)->owner;
    const Identity::Uuid contract(requested);
    if (contract == Value::Abstract::contract_id) {
      const Value::Abstract::Api api(&provider.typed, typed_operations);
      *Data::cast<Value::Abstract::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (contract == Abstract::contract_id) {
      *Data::cast<ttx_abstract>(output) = provider.get_abstract().get_abi();
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (!requested.high && requested.low <= 1) {
      return TTX_NEGOTIATION_REJECTED;
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto resolve_route(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    (void)context;
    (void)route;
    return ttx_none();
  }

  static auto visit_routes(void* context, ttx_graph_visitor visitor) -> void {
    (void)context;
    (void)visitor;
  }

  static auto get_state(void* context) -> ttx_abstract_state {
    (void)context;
    return TTX_ABSTRACT_PRESENT;
  }

  static auto get_typed(void* context) -> const void* {
    return Data::cast<TypedContext>(context)->value;
  }

  static auto reenter(void* context) -> ttx_abstract {
    return Data::cast<TypedContext>(context)->owner->get_abstract().get_abi();
  }

  inline static constexpr Value::Abstract::Operations typed_operations =
      Value::Abstract::Operations(get_typed, reenter);

  inline static constexpr ttx_abstract_ops graph_operations = {
    supports, bind, resolve_route, visit_routes, get_state,
  };

  GraphContext graph;
  TypedContext typed;
};

static Harness ProjectionTests = {.name = "TTX::Projection"};

VALIDATION_TEST(ProjectionTests, generated_contracts) {
  auto value = Value::create(42);
  const auto abstract = value.get_abstract();

  EXPECT(abstract.get_state() == Abstract::State::Present);
  EXPECT(
      abstract.supports(Value::Abstract::contract_id) ==
      Negotiation::Status::Satisfied);
  EXPECT(abstract.supports<Abstract>() == Negotiation::Status::Satisfied);
  EXPECT(abstract.supports<Signal>() == Negotiation::Status::Satisfied);
  EXPECT(abstract.supports<BoundValue>() == Negotiation::Status::Unknown);

  auto projected = Value::Abstract::convert(abstract);
  projected.visit(
      [&](const Value::Abstract& projection) {
        EXPECT_EQ(projection.get_typed().get_value(), U64(42));
        EXPECT_EQ(projection.get_abstract(), abstract);
      },

      [&] { EXPECT(False); });

  abstract.bind<Abstract>().visit(
      [&](Abstract rebound) { EXPECT_EQ(rebound, abstract); },
      [&] { EXPECT(False); });

  EXPECT_NOT(abstract.bind<Signal>());
  abstract.bind<BoundValue>().visit(
      [&](BoundValue bound) { EXPECT_EQ(bound.get_value(), U64(42)); },
      [&] { EXPECT(False); });

  EXPECT(abstract.resolve_route({}).get_state() == Abstract::State::None);

  Count visited = 0;
  auto visitor = [&](View::Bytes, Abstract) { ++visited; };
  abstract.visit_routes(Abstract::Visitor(visitor));
  EXPECT_EQ(visited, Count(0));
}

VALIDATION_TEST(ProjectionTests, provider_owned_materialization) {
  auto value = Value::create(7);
  Simulacrum provider;
  provider.initialize(&value);

  const auto graph = provider.get_abstract();

  auto projected = Value::Abstract::convert(graph);
  projected.visit(
      [&](const Value::Abstract& projection) {
        EXPECT(projection.get_abi().context != graph.get_abi().context);
        EXPECT_EQ(projection.get_typed().get_value(), U64(7));
        EXPECT_EQ(projection.get_abstract(), graph);
      },

      [&] { EXPECT(False); });
}
