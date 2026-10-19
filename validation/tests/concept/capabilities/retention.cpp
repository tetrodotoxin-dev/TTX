// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/view/null_terminated.hpp"
#include "perimortem/memory/allocator/arena.hpp"

#include "ttx/concept/capabilities/borrow.hpp"
#include "ttx/concept/capabilities/callable.hpp"
#include "ttx/concept/capabilities/create.hpp"
#include "ttx/concept/policies/none.hpp"
#include "ttx/semantic/realization/invocation.h"
#include "validation/providers/retention/provider.h"
#include "validation/support/library.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

static Toolchain::Validation::Harness Retention = {
  .name = "TTX::RetainedCreation"};

class ForeignRetention {
 public:
  System::Process::Library library =
      Validation::Support::open_library("libretention_provider.so"_view);
  retention_fixture api = reinterpret_cast<decltype(&retention_provider_open)>(
      Validation::Support::find_symbol(
          library,
          "retention_provider_open"_view))();
};

// The creation callback ends before any operation is bound or called. Only
// the negotiated Borrowed contract carries access across that boundary.
VALIDATION_TEST(Retention, create_borrow_invoke_release) {
  ForeignRetention foreign;
  Memory::Allocator::Arena errors;
  foreign.library.symbol("retention_invoke"_view, errors)
      .visit([&](void*) { EXPECT(False); }, [](Core::View::Bytes) {});

  const Abstract factory(foreign.api.factory);
  EXPECT(
      factory.supports<Capabilities::Create>() == Binding::Status::Satisfied);
  EXPECT_EQ(foreign.api.statistics().callbacks, Count(0));

  Core::Storage::Option<Policies::Borrowed> retained;
  auto callback = [&](Abstract created) {
    EXPECT_EQ(foreign.api.statistics().inside_callback, U8(1));
    created.bind<Capabilities::Borrow>().visit(
        [&](Capabilities::Borrow borrow) {
          EXPECT_EQ(foreign.api.statistics().acquisitions, Count(0));
          borrow.borrow().visit(
              [&](Policies::Borrowed outcome) {
                EXPECT(outcome.get_abi().context != created.get_abi().context);
                retained = outcome;
              },

              [&](Binding::Status) { EXPECT(False); });
        },

        [&](Binding::Status) { EXPECT(False); });
  };

  factory.bind<Capabilities::Create>().visit(
      [&](Capabilities::Create create) {
        EXPECT(
            create.create(
                Policies::None::get_none().get_abstract(), callback) ==
            Binding::Status::Satisfied);
      },

      [&](Binding::Status) { EXPECT(False); });

  ASSERT(retained);
  EXPECT_EQ(foreign.api.statistics().callbacks, Count(1));
  EXPECT_EQ(foreign.api.statistics().inside_callback, U8(0));
  EXPECT_EQ(foreign.api.statistics().live, Count(1));

  // Copying and binding carry the existing release obligation. Both continue
  // through the retained Abstract, including when another capability supplies
  // its own Abstract operations.
  retained.visit(
      [&](Policies::Borrowed copied) {
        copied.get_abstract().bind<Policies::Borrowed>().visit(
            [&](Policies::Borrowed view) {
              EXPECT(
                  view.get_abstract().supports<Capabilities::Callable>() ==
                  Binding::Status::Satisfied);
            },

            [&](Binding::Status) { EXPECT(False); });
        EXPECT_EQ(foreign.api.statistics().acquisitions, Count(1));

        copied.get_abstract().bind<Capabilities::Callable>().visit(
            [&](Capabilities::Callable callable) {
              EXPECT(
                  callable.get_abstract().supports<Policies::Borrowed>() ==
                  Binding::Status::Satisfied);
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
                      api_form, reinterpret_cast<U8*>(&operation),
                      sizeof(operation)};

                    ASSERT_EQ(ttx_storage_check(target), TTX_DATA_SUCCESS);

                    const auto status = callable.get_abstract().bind_interface(
                        description.get_contract(),
                        Ttx::Data::Form::Storage(target));
                    ASSERT(status == Binding::Status::Satisfied);
                    ASSERT(operation.invoke);

                    const auto& integer =
                        Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
                            U64>::reference>::get_representation();
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

                  [&](Binding::Status) { EXPECT(False); });
            },

            [&](Binding::Status) { EXPECT(False); });

        // A second Borrow creates its own obligation even when it returns the
        // same context. After releasing the first, only the second acquisition
        // is used.
        Core::Storage::Option<Policies::Borrowed> second;
        copied.get_abstract().bind<Capabilities::Borrow>().visit(
            [&](Capabilities::Borrow borrow) {
              borrow.borrow().visit(
                  [&](Policies::Borrowed outcome) { second = outcome; },
                  [&](Binding::Status) { EXPECT(False); });
            },

            [&](Binding::Status) { EXPECT(False); });
        EXPECT(second);
        EXPECT_EQ(foreign.api.statistics().acquisitions, Count(2));
        copied.release();

        second.visit(
            [&](Policies::Borrowed acquired) {
              EXPECT_EQ(foreign.api.statistics().live, Count(1));
              EXPECT(
                  acquired.get_abstract().supports<Capabilities::Callable>() ==
                  Binding::Status::Satisfied);
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
  const ttx_binding_status failures[] = {
    TTX_BINDING_UNKNOWN, TTX_BINDING_REJECTED};

  for (const auto failure : failures) {
    foreign.api.configure(failure, TTX_BINDING_SATISFIED);

    auto unexpected = [&](Abstract) { EXPECT(False); };
    factory.bind<Capabilities::Create>().visit(
        [&](Capabilities::Create create) {
          EXPECT(
              create.create(
                  Policies::None::get_none().get_abstract(), unexpected) ==
              static_cast<Binding::Status>(failure));
        },

        [&](Binding::Status) { EXPECT(False); });
    EXPECT_EQ(foreign.api.statistics().callbacks, Count(0));

    foreign.api.configure(TTX_BINDING_SATISFIED, failure);

    auto callback = [&](Abstract created) {
      created.bind<Capabilities::Borrow>().visit(
          [&](Capabilities::Borrow borrow) {
            borrow.borrow().visit(
                [&](Policies::Borrowed outcome) {
                  EXPECT(False);
                  outcome.release();
                },

                [&](Binding::Status outcome) {
                  EXPECT(outcome == static_cast<Binding::Status>(failure));
                });
          },

          [&](Binding::Status) { EXPECT(False); });
    };

    factory.bind<Capabilities::Create>().visit(
        [&](Capabilities::Create create) {
          EXPECT(
              create.create(
                  Policies::None::get_none().get_abstract(), callback) ==
              Binding::Status::Satisfied);
        },

        [&](Binding::Status) { EXPECT(False); });
    EXPECT_EQ(foreign.api.statistics().callbacks, Count(1));
    EXPECT_EQ(foreign.api.statistics().acquisitions, Count(0));
    EXPECT_EQ(foreign.api.statistics().releases, Count(0));
    EXPECT_EQ(foreign.api.statistics().live, Count(0));
  }
}
