// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/capabilities/create.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"

#include "ttx/concept/capabilities/borrow.hpp"
#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/unknown.h"
#include "ttx/data/transfer.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Serialization;
using namespace Toolchain::Validation;
using namespace Ttx::Concept;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness Creation = {.name = "TTX::Create"};

class ScopedCreate {
 public:
  Negotiation::Status status = Negotiation::Status::Satisfied;
  bool observing = false;

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    const Formats::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Capabilities::Create::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage target)
      -> ttx_negotiation_status {
    if (Formats::Uuid(id) == Capabilities::Create::contract_id) {
      Capabilities::Create::Api api = {context, &operations};
      return Transfer::copy(Form::Storage::from(api), Form::Storage(target)) ==
                     Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    if (Formats::Uuid(id) == Abstract::contract_id) {
      Abstract::Api api = resolve(context);
      return Transfer::copy(Form::Storage::from(api), Form::Storage(target)) ==
                     Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto data(void* context) -> perimortem_view_bytes { return {}; }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations.abstract};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    return ttx_unknown();
  }

  static auto visit(void* context, ttx_concept_visitor callback) -> void {}

  struct Created {
    U8 value = 42;

    static auto supports(void* context, perimortem_uuid id)
        -> ttx_negotiation_status {
      return Formats::Uuid(id) == Abstract::contract_id
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_UNKNOWN;
    }

    static auto bind(void* context, perimortem_uuid id, ttx_storage target)
        -> ttx_negotiation_status {
      if (supports(context, id) != TTX_NEGOTIATION_SATISFIED) {
        return TTX_NEGOTIATION_UNKNOWN;
      }

      Abstract::Api api = resolve(context);
      return Transfer::copy(Form::Storage::from(api), Form::Storage(target)) ==
                     Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    static auto data(void* context) -> perimortem_view_bytes {
      return {&Data::cast<Created>(context)->value, 1};
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
      void (*callback)(void*, ttx_abstract)) -> ttx_negotiation_status {
    auto& subject = *Data::cast<ScopedCreate>(context);
    if (subject.status != Negotiation::Status::Satisfied) {
      return ttx_negotiation_status(subject.status);
    }

    Created created;
    subject.observing = true;
    callback(callback_context, Created::resolve(&created));
    subject.observing = false;
    return TTX_NEGOTIATION_SATISFIED;
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
                Negotiation::Status::Unknown);

            U8 copied = 0;
            Count visits = 0;
            auto callback = [&](Abstract subject) {
              EXPECT(provider.observing);
              EXPECT(
                  subject.supports<Capabilities::Borrow>() ==
                  Negotiation::Status::Unknown);
              copied = subject.get_data()[0];
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

          [&](Negotiation::Status) { EXPECT(False); });
}
