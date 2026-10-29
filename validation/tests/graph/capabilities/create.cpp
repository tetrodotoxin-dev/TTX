// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/capabilities/create.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"

#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.h"
#include "validation/tests/graph/fixtures/value.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness Creation = {.name = "TTX::Create"};

class ScopedCreate {
 public:
  Negotiation::Status status = Negotiation::Status::Satisfied;
  bool observing = false;

  static auto state(void* context) -> ttx_abstract_state {
    return TTX_ABSTRACT_PRESENT;
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    const Identity::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Capabilities::Create::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    if (Identity::Uuid(id) == Capabilities::Create::contract_id) {
      Capabilities::Create::Api api = {context, &operations};
      *Data::cast<Capabilities::Create::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (Identity::Uuid(id) == Abstract::contract_id) {
      Abstract::Api api = get_abstract(context);
      *Data::cast<Abstract::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto get_abstract(void* context) -> ttx_abstract {
    return {context, &graph_operations};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    return ttx_unknown();
  }

  static auto visit(void* context, ttx_graph_visitor callback) -> void {}

  struct Created {
    U8 value = 42;

    static auto state(void* context) -> ttx_abstract_state {
      return TTX_ABSTRACT_PRESENT;
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
      return Data::cast<Created>(context)->value;
    }

    static auto get_abstract(void* context) -> ttx_abstract {
      return {context, &operations};
    }

    static constexpr ttx_abstract_ops operations = {
      supports, bind, lookup, visit, state};
  };

  static auto create(
      void* context,
      ttx_abstract,
      void* callback_context,
      void (*callback)(void*, ttx_abstract)) -> ttx_negotiation_status {
    auto& subject = *Data::cast<ScopedCreate>(context);
    if (subject.status != Negotiation::Status::Satisfied) {
      return ttx_negotiation_status(subject.status);
    }

    Created created;
    subject.observing = true;
    callback(callback_context, Created::get_abstract(&created));
    subject.observing = false;
    return TTX_NEGOTIATION_SATISFIED;
  }

  static constexpr ttx_abstract_ops graph_operations = {
    supports, bind, lookup, visit, state};

  static constexpr ttx_create_ops operations = {create};
};

// The created value lives on the provider's stack. Consuming it during the
// callback needs only Create, while keeping the copied byte uses caller
// storage.
VALIDATION_TEST(Creation, scoped_answer) {
  ScopedCreate provider;
  Abstract(ScopedCreate::get_abstract(&provider))
      .bind<Capabilities::Create>()
      .visit(
          [&](Capabilities::Create create) {
            U8 copied = 0;
            Count visits = 0;
            auto callback = [&](Abstract subject) {
              EXPECT(provider.observing);
              copied = subject.bind<Validation::Value>().visit(
                  [](Validation::Value value) { return value.read(); },
                  [&] {
                    EXPECT(False);
                    return U8(0);
                  });

              ++visits;
            };

            EXPECT(
                create.create(
                    Policies::None::get_none().get_abstract(), callback) ==
                Negotiation::Status::Satisfied);
            EXPECT_NOT(provider.observing);
            EXPECT_EQ(copied, U8(42));

            const Negotiation::Status failures[] = {
              Negotiation::Status::Unknown, Negotiation::Status::Rejected};

            for (const auto failure : failures) {
              provider.status = failure;
              EXPECT(
                  create.create(
                      Policies::None::get_none().get_abstract(), callback) ==
                  failure);
            }

            EXPECT_EQ(visits, Count(1));
          },

          [&]() { EXPECT(False); });
}

VALIDATION_TEST(Creation, present_bindings) {
  ScopedCreate factory;
  ScopedCreate::Created value;
  const ttx_abstract graphs[] = {
    ScopedCreate::get_abstract(&factory),
    ScopedCreate::Created::get_abstract(&value),
  };

  const perimortem_uuid ids[] = {
    {TTX_NONE_ID_HIGH, TTX_NONE_ID_LOW},
    {TTX_UNKNOWN_ID_HIGH, TTX_UNKNOWN_ID_LOW},
  };

  for (const auto api : graphs) {
    EXPECT_EQ(api.operations->get_state(api.context), TTX_ABSTRACT_PRESENT);
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
