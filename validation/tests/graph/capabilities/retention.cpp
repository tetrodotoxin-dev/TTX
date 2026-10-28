// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/graph/capabilities/create.hpp"
#include "ttx/graph/capabilities/retain.hpp"
#include "ttx/graph/policies/none.hpp"
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

// The factory, callback result and retained result all remain Present. Each
// must reject both reserved terminal UUIDs even though their ordinary
// capabilities and access durations differ.
VALIDATION_TEST(Retention, present_bindings) {
  ForeignRetention foreign;
  const auto check = [&](Abstract graph) {
    EXPECT(graph.get_state() == Abstract::State::Present);

    const perimortem_uuid ids[] = {
      {TTX_NONE_ID_HIGH, TTX_NONE_ID_LOW},
      {TTX_UNKNOWN_ID_HIGH, TTX_UNKNOWN_ID_LOW},
    };

    const auto api = graph.get_abi();
    for (const auto id : ids) {
      EXPECT_EQ(
          api.operations->supports(api.context, id), TTX_NEGOTIATION_REJECTED);

      ttx_abstract output{};
      EXPECT_EQ(
          api.operations->bind(api.context, id, &output),
          TTX_NEGOTIATION_REJECTED);
    }
  };

  const Abstract factory(foreign.api.factory);
  check(factory);

  Storage::Option<Policies::Retained> retained;
  auto callback = [&](Abstract created) {
    check(created);
    created.bind<Capabilities::Retain>().visit(
        [&](Capabilities::Retain request) {
          request.retain().visit(
              [&](Policies::Retained value) { retained = value; },
              [&](Negotiation::Status) { EXPECT(False); });
        },

        [&] { EXPECT(False); });
  };

  factory.bind<Capabilities::Create>().visit(
      [&](Capabilities::Create create) {
        EXPECT(
            create.create(Abstract(ttx_none()), callback) ==
            Negotiation::Status::Satisfied);
      },

      [&] { EXPECT(False); });
  ASSERT(retained);
  retained.visit(
      [&](Policies::Retained value) {
        check(value.get_abstract());
        EXPECT(value.release() == Policies::Retained::Status::Success);
      },

      [&] { EXPECT(False); });
  EXPECT_EQ(foreign.api.statistics().live, Count(0));
}

// Retention moves the callback result into provider storage. Copies of one
// Retained record share its release obligation, while each further retain call
// creates an independent acquisition.
VALIDATION_TEST(Retention, create_retain_release) {
  ForeignRetention foreign;
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

        [&]() { EXPECT(False); });
  };

  factory.bind<Capabilities::Create>().visit(
      [&](Capabilities::Create create) {
        EXPECT(
            create.create(
                Policies::None::get_none().get_abstract(), callback) ==
            Negotiation::Status::Satisfied);
      },

      [&]() { EXPECT(False); });

  ASSERT(retained);
  EXPECT_EQ(foreign.api.statistics().callbacks, Count(1));
  EXPECT_EQ(foreign.api.statistics().inside_callback, U8(0));
  EXPECT_EQ(foreign.api.statistics().live, Count(1));

  // After the creation callback ends, the consumer copies the Retained record.
  // Both copies identify the same acquisition and its single release
  // obligation.
  retained.visit(
      [&](Policies::Retained copied) {
        copied.get_abstract().bind<Policies::Retained>().visit(
            [&](Policies::Retained view) {
              EXPECT(
                  view.get_abstract().supports<Capabilities::Retain>() ==
                  Negotiation::Status::Satisfied);
            },

            [&]() { EXPECT(False); });
        EXPECT_EQ(foreign.api.statistics().acquisitions, Count(1));

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

            [&]() { EXPECT(False); });
        EXPECT(second);
        EXPECT_EQ(foreign.api.statistics().acquisitions, Count(2));
        EXPECT(copied.release() == Policies::Retained::Status::Success);

        second.visit(
            [&](Policies::Retained acquired) {
              EXPECT_EQ(foreign.api.statistics().live, Count(1));
              EXPECT(
                  acquired.get_abstract().supports<Policies::Retained>() ==
                  Negotiation::Status::Satisfied);
              EXPECT(acquired.release() == Policies::Retained::Status::Success);
            },

            [&] { EXPECT(False); });
      },

      [&] { EXPECT(False); });

  EXPECT_EQ(foreign.api.statistics().releases, Count(2));
  EXPECT_EQ(foreign.api.statistics().live, Count(0));
}

// The first release leaves an independent acquisition usable. Final release
// frees the C provider's allocation before returning either cleanup outcome.
// The consumer checks the outcome through value data and fixture statistics.
VALIDATION_TEST(Retention, cleanup_outcome) {
  ForeignRetention foreign;
  const ttx_release_status outcomes[] = {
    TTX_RELEASE_SUCCESS, TTX_RELEASE_CLEANUP_FAILED};

  for (const auto expected : outcomes) {
    foreign.api.configure(TTX_NEGOTIATION_SATISFIED, TTX_NEGOTIATION_SATISFIED);
    foreign.api.configure_cleanup(expected);

    Storage::Option<Policies::Retained> first;
    auto callback = [&](Abstract created) {
      created.bind<Capabilities::Retain>().visit(
          [&](Capabilities::Retain request) {
            request.retain().visit(
                [&](Policies::Retained acquired) { first = acquired; },
                [&](Negotiation::Status) { EXPECT(False); });
          },

          [&] { EXPECT(False); });
    };

    Abstract(foreign.api.factory)
        .bind<Capabilities::Create>()
        .visit(
            [&](Capabilities::Create create) {
              EXPECT(
                  create.create(Abstract(ttx_none()), callback) ==
                  Negotiation::Status::Satisfied);
            },

            [&] { EXPECT(False); });
    ASSERT(first);
    EXPECT_EQ(foreign.api.statistics().inside_callback, U8(0));

    first.visit(
        [&](Policies::Retained acquired) {
          acquired.get_abstract().bind<Capabilities::Retain>().visit(
              [&](Capabilities::Retain request) {
                request.retain().visit(
                    [&](Policies::Retained second) {
                      const auto api = acquired.get_abi();
                      EXPECT_EQ(
                          api.operations->release(api.context),
                          TTX_RELEASE_SUCCESS);
                      EXPECT_EQ(foreign.api.statistics().live, Count(1));
                      EXPECT_EQ(foreign.api.statistics().cleanups, Count(0));
                      EXPECT(
                          second.get_abstract()
                              .supports<Policies::Retained>() ==
                          Negotiation::Status::Satisfied);

                      const auto outcome = second.release();
                      EXPECT(outcome == Policies::Retained::Status(expected));
                    },

                    [&](Negotiation::Status) {
                      EXPECT(False);
                      acquired.release();
                    });
              },

              [&] {
                EXPECT(False);
                acquired.release();
              });
        },

        [&] { EXPECT(False); });

    EXPECT_EQ(foreign.api.statistics().acquisitions, Count(2));
    EXPECT_EQ(foreign.api.statistics().releases, Count(2));
    EXPECT_EQ(foreign.api.statistics().cleanups, Count(1));
    EXPECT_EQ(foreign.api.statistics().live, Count(0));
  }
}

// The earlier UUIDs specify a void release operation. A consumer must obtain
// the new contracts before interpreting the operation's returned status.
VALIDATION_TEST(Retention, release_contract_identity) {
  ForeignRetention foreign;
  const perimortem_uuid earlier[] = {
    {0xe028b547172140acULL, 0xac4e943287c34557ULL},
    {0x36e9d1a476ab4efcULL, 0x950306045c56e03dULL},
  };

  auto check = [&](Abstract abstract) {
    const auto api = abstract.get_abi();
    for (const auto id : earlier) {
      EXPECT(
          api.operations->supports(api.context, id) !=
          TTX_NEGOTIATION_SATISFIED);

      ttx_retained output{};
      EXPECT(
          api.operations->bind(api.context, id, &output) !=
          TTX_NEGOTIATION_SATISFIED);
    }
  };

  auto callback = [&](Abstract created) {
    check(created);
    created.bind<Capabilities::Retain>().visit(
        [&](Capabilities::Retain request) {
          request.retain().visit(
              [&](Policies::Retained acquired) {
                check(acquired.get_abstract());
                EXPECT(
                    acquired.release() == Policies::Retained::Status::Success);
              },

              [&](Negotiation::Status) { EXPECT(False); });
        },

        [&] { EXPECT(False); });
  };

  Abstract(foreign.api.factory)
      .bind<Capabilities::Create>()
      .visit(
          [&](Capabilities::Create create) {
            EXPECT(
                create.create(Abstract(ttx_none()), callback) ==
                Negotiation::Status::Satisfied);
          },

          [&] { EXPECT(False); });
  EXPECT_EQ(foreign.api.statistics().live, Count(0));
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

        [&]() { EXPECT(False); });
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

          [&]() { EXPECT(False); });
    };

    factory.bind<Capabilities::Create>().visit(
        [&](Capabilities::Create create) {
          EXPECT(
              create.create(
                  Policies::None::get_none().get_abstract(), callback) ==
              Negotiation::Status::Satisfied);
        },

        [&]() { EXPECT(False); });
    EXPECT_EQ(foreign.api.statistics().callbacks, Count(1));
    EXPECT_EQ(foreign.api.statistics().acquisitions, Count(0));
    EXPECT_EQ(foreign.api.statistics().releases, Count(0));
    EXPECT_EQ(foreign.api.statistics().live, Count(0));
  }
}
