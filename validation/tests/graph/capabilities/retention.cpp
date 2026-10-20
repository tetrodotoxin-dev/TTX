// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"
#include "perimortem/memory/allocator/arena.hpp"

#include "ttx/graph/capabilities/callable.hpp"
#include "ttx/graph/capabilities/create.hpp"
#include "ttx/graph/capabilities/retain.hpp"
#include "ttx/graph/policies/none.hpp"
#include "ttx/semantic/realization/invocation.h"
#include "validation/providers/retention/provider.h"
#include "validation/support/library.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Perimortem::System;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness Retention = {.name = "TTX::RetainedCreation"};

class ForeignRetention {
 public:
  Process::Library library =
      Validation::Support::open_library("libretention_provider.so"_view);
  retention_fixture api = Data::cast<decltype(retention_provider_open)>(
      Validation::Support::find_symbol(
          library,
          "retention_provider_open"_view))();
};

// The increment operation is bound and called after the creation callback
// returns. The retained Abstract must still describe and bind that operation.
VALIDATION_TEST(Retention, create_retain_invoke_release) {
  ForeignRetention foreign;
  Allocator::Arena errors;
  foreign.library.symbol("retention_invoke"_view, errors)
      .visit([&](void*) { EXPECT(False); }, [](View::Bytes) {});

  const Abstract factory(foreign.api.factory);
  EXPECT(
      factory.supports<Capabilities::Create>() ==
      Negotiation::Status::Satisfied);
  EXPECT_EQ(foreign.api.statistics().callbacks, Count(0));

  Storage::Option<Policies::Retained> retained;
  auto callback = [&](Abstract created) {
    EXPECT_EQ(foreign.api.statistics().inside_callback, U8(1));
    created.bind<Capabilities::Retain>().visit(
        [&](Capabilities::Retain request) {
          EXPECT_EQ(foreign.api.statistics().acquisitions, Count(0));
          request.retain().visit(
              [&](Policies::Retained outcome) {
                EXPECT(outcome.get_abi().context != created.get_abi().context);
                retained = outcome;
              },

              [&](Negotiation::Status) { EXPECT(False); });
        },

        [&](Negotiation::Status) { EXPECT(False); });
  };

  factory.bind<Capabilities::Create>().visit(
      [&](Capabilities::Create create) {
        EXPECT(
            create.create(
                Policies::None::get_none().get_abstract(), callback) ==
            Negotiation::Status::Satisfied);
      },

      [&](Negotiation::Status) { EXPECT(False); });

  ASSERT(retained);
  EXPECT_EQ(foreign.api.statistics().callbacks, Count(1));
  EXPECT_EQ(foreign.api.statistics().inside_callback, U8(0));
  EXPECT_EQ(foreign.api.statistics().live, Count(1));

  // After the creation callback ends, the consumer copies the Retained record
  // and binds Callable through it. The provider keeps the operation callable
  // through those interfaces until release. They share the original
  // acquisition and its single release obligation.
  retained.visit(
      [&](Policies::Retained copied) {
        copied.get_abstract().bind<Policies::Retained>().visit(
            [&](Policies::Retained view) {
              EXPECT(
                  view.get_abstract().supports<Capabilities::Callable>() ==
                  Negotiation::Status::Satisfied);
            },

            [&](Negotiation::Status) { EXPECT(False); });
        EXPECT_EQ(foreign.api.statistics().acquisitions, Count(1));

        copied.get_abstract().bind<Capabilities::Callable>().visit(
            [&](Capabilities::Callable callable) {
              EXPECT(
                  callable.get_abstract().supports<Policies::Retained>() ==
                  Negotiation::Status::Satisfied);
              callable.describe().visit(
                  [&](Capabilities::Callable::Description description) {
                    // The consumer establishes the API it can actually call. A
                    // provider's description alone cannot establish that native
                    // ABI.
                    const auto* api_form = ttx_invocation_representation();
                    ASSERT(
                        description.get_representation().compatible(*api_form));

                    ttx_invocation operation = {};
                    const ttx_storage target = {
                      api_form, Data::cast<U8>(&operation), sizeof(operation)};

                    ASSERT_EQ(ttx_storage_check(target), TTX_DATA_SUCCESS);

                    const auto status = callable.get_abstract().bind_interface(
                        description.get_contract(), Form::Storage(target));
                    ASSERT(status == Negotiation::Status::Satisfied);
                    ASSERT(operation.invoke);

                    const auto& integer = Form::Compiled<
                        Form::Native<U64>::reference>::get_representation();
                    const Bool compatible =
                        operation.inputs && operation.outputs &&
                        integer.compatible(*operation.inputs) &&
                        integer.compatible(*operation.outputs) &&
                        integer.compatible(
                            description.get_inputs().get_representation()) &&
                        integer.compatible(
                            description.get_outputs().get_representation());
                    ASSERT(compatible);

                    const U64 amount = 1;
                    U64 outcome = 0;
                    ASSERT_EQ(
                        operation.invoke(operation.context, &amount, &outcome),
                        TTX_DATA_SUCCESS);
                    EXPECT_EQ(outcome, U64(42));
                    ASSERT_EQ(
                        operation.invoke(operation.context, &amount, &outcome),
                        TTX_DATA_SUCCESS);
                    EXPECT_EQ(outcome, U64(43));
                  },

                  [&](Negotiation::Status) { EXPECT(False); });
            },

            [&](Negotiation::Status) { EXPECT(False); });

        // A second `retain` call creates its own release obligation even when
        // both Retained results use the same context. After releasing the first
        // result, the consumer uses only the second.
        Storage::Option<Policies::Retained> second;
        copied.get_abstract().bind<Capabilities::Retain>().visit(
            [&](Capabilities::Retain request) {
              request.retain().visit(
                  [&](Policies::Retained outcome) { second = outcome; },
                  [&](Negotiation::Status) { EXPECT(False); });
            },

            [&](Negotiation::Status) { EXPECT(False); });
        EXPECT(second);
        EXPECT_EQ(foreign.api.statistics().acquisitions, Count(2));
        copied.release();

        second.visit(
            [&](Policies::Retained acquired) {
              EXPECT_EQ(foreign.api.statistics().live, Count(1));
              EXPECT(
                  acquired.get_abstract().supports<Capabilities::Callable>() ==
                  Negotiation::Status::Satisfied);
              acquired.release();
            },

            [&] { EXPECT(False); });
      },

      [&] { EXPECT(False); });

  EXPECT_EQ(foreign.api.statistics().releases, Count(2));
  EXPECT_EQ(foreign.api.statistics().live, Count(0));
  EXPECT_EQ(foreign.api.statistics().calls, Count(2));
}

VALIDATION_TEST(Retention, refused_creation_and_retention) {
  ForeignRetention foreign;
  const Abstract factory(foreign.api.factory);
  const ttx_negotiation_status failures[] = {
    TTX_NEGOTIATION_UNKNOWN, TTX_NEGOTIATION_REJECTED};

  for (const auto failure : failures) {
    foreign.api.configure(failure, TTX_NEGOTIATION_SATISFIED);

    auto unexpected = [&](Abstract) { EXPECT(False); };
    factory.bind<Capabilities::Create>().visit(
        [&](Capabilities::Create create) {
          EXPECT(
              create.create(
                  Policies::None::get_none().get_abstract(), unexpected) ==
              Negotiation::Status(failure));
        },

        [&](Negotiation::Status) { EXPECT(False); });
    EXPECT_EQ(foreign.api.statistics().callbacks, Count(0));

    foreign.api.configure(TTX_NEGOTIATION_SATISFIED, failure);

    auto callback = [&](Abstract created) {
      created.bind<Capabilities::Retain>().visit(
          [&](Capabilities::Retain request) {
            request.retain().visit(
                [&](Policies::Retained outcome) {
                  EXPECT(False);
                  outcome.release();
                },

                [&](Negotiation::Status outcome) {
                  EXPECT(outcome == Negotiation::Status(failure));
                });
          },

          [&](Negotiation::Status) { EXPECT(False); });
    };

    factory.bind<Capabilities::Create>().visit(
        [&](Capabilities::Create create) {
          EXPECT(
              create.create(
                  Policies::None::get_none().get_abstract(), callback) ==
              Negotiation::Status::Satisfied);
        },

        [&](Negotiation::Status) { EXPECT(False); });
    EXPECT_EQ(foreign.api.statistics().callbacks, Count(1));
    EXPECT_EQ(foreign.api.statistics().acquisitions, Count(0));
    EXPECT_EQ(foreign.api.statistics().releases, Count(0));
    EXPECT_EQ(foreign.api.statistics().live, Count(0));
  }
}
