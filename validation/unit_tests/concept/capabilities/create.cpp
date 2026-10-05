// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/capabilities/create.hpp"

#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/capabilities/borrow.hpp"
#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/unknown.h"

using namespace Perimortem;
using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

static Toolchain::Validation::Harness Creation = {.name = "TTX::Create"};

class ScopedCreate {
 public:
  Binding::Status status = Binding::Status::Satisfied;
  bool observing = false;

  static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
    const System::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Capabilities::Create::contract_id
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage target)
      -> ttx_binding_status {
    if (System::Uuid(id) == Capabilities::Create::contract_id) {
      return static_cast<ttx_binding_status>(
          Binding::provide<Capabilities::Create>(
              {context, &operations}, Ttx::Data::Form::Storage(target)));
    }

    if (System::Uuid(id) == Abstract::contract_id) {
      return static_cast<ttx_binding_status>(Binding::provide<Abstract>(
          resolve(context), Ttx::Data::Form::Storage(target)));
    }

    return TTX_BINDING_UNKNOWN;
  }

  static auto data(void*) -> perimortem_view_bytes { return {}; }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations.abstract};
  }

  static auto lookup(void*, perimortem_view_bytes) -> ttx_abstract {
    return ttx_unknown();
  }

  static void visit(void*, ttx_concept_visitor) {}

  struct Created {
    U8 value = 42;

    static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
      return System::Uuid(id) == Abstract::contract_id ? TTX_BINDING_SATISFIED
                                                       : TTX_BINDING_UNKNOWN;
    }

    static auto bind(void* context, perimortem_uuid id, ttx_storage target)
        -> ttx_binding_status {
      if (supports(context, id) != TTX_BINDING_SATISFIED) {
        return TTX_BINDING_UNKNOWN;
      }

      return static_cast<ttx_binding_status>(Binding::provide<Abstract>(
          resolve(context), Ttx::Data::Form::Storage(target)));
    }

    static auto data(void* context) -> perimortem_view_bytes {
      return {&static_cast<Created*>(context)->value, 1};
    }

    static auto resolve(void* context) -> ttx_abstract {
      return {context, &operations};
    }

    static constexpr ttx_abstract_ops operations = {supports, bind,   data,
                                                    resolve,  lookup, visit};
  };

  static auto create(
      void* context,
      ttx_abstract,
      void* callback_context,
      void (*callback)(void*, ttx_abstract)) -> ttx_binding_status {
    auto& subject = *static_cast<ScopedCreate*>(context);
    if (subject.status != Binding::Status::Satisfied) {
      return static_cast<ttx_binding_status>(subject.status);
    }

    Created created;
    subject.observing = true;
    callback(callback_context, Created::resolve(&created));
    subject.observing = false;
    return TTX_BINDING_SATISFIED;
  }

  static constexpr ttx_create_ops operations = {
    {supports, bind, data, resolve, lookup, visit},
    create};
};

// The created value lives on the provider's stack. Consuming it during the
// callback needs only Create, while keeping the copied byte uses caller
// storage.
VALIDATION_TEST(Creation, scoped_answer) {
  ScopedCreate provider;
  Abstract(ScopedCreate::resolve(&provider))
      .bind<Capabilities::Create>()
      .visit(
          [&](Capabilities::Create create) {
            EXPECT(
                create.get_abstract().supports<Capabilities::Borrow>() ==
                Binding::Status::Unknown);

            U8 copied = 0;
            Count visits = 0;
            auto callback = [&](Abstract subject) {
              EXPECT(provider.observing);
              EXPECT(
                  subject.supports<Capabilities::Borrow>() ==
                  Binding::Status::Unknown);
              copied = subject.get_data()[0];
              ++visits;
            };
            EXPECT(
                create.create(
                    Policies::None::get_none().get_abstract(), callback) ==
                Binding::Status::Satisfied);
            EXPECT_NOT(provider.observing);
            EXPECT_EQ(copied, U8(42));

            const Binding::Status failures[] = {
              Binding::Status::Unknown, Binding::Status::Rejected};
            for (const auto failure : failures) {
              provider.status = failure;
              EXPECT(
                  create.create(
                      Policies::None::get_none().get_abstract(), callback) ==
                  failure);
            }

            EXPECT_EQ(visits, Count(1));
          },
          [&](Binding::Failure) { EXPECT(False); });
}
