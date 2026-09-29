// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/static/vector.hpp"

#include "tests/semantic/fixtures.hpp"
#include "ttx/data/protocol/consumer.hpp"
#include "ttx/data/protocol/direct/provider.hpp"

using namespace Validation::FlowTests;

// The default consumer supplies one API under each accepted consumer UUID.
// Matching that API does not grant the provider side of any agreement.
VALIDATION_TEST(TtxFlow, consumer_contracts) {
  using Ttx::Data::Protocol::Consumer;
  Preparation prepare;
  const auto& four = prepare(four_schema);
  const auto query = Flow::consumer(four);
  const Static::Vector<Flow::Contracts, 4> contracts = {
    {
      Flow::direct,
      Flow::shared,
      Flow::block,
      Flow::fragment,
    },
  };
  for (const auto& pair : contracts.get_view()) {
    query.bind<Consumer>(pair.consumer)
        .visit(
            [&](Consumer consumer) {
              EXPECT(&consumer.get_representation() == &four);
            },
            [&](Binding::Failure) { EXPECT(false); });
    query.bind<Consumer>(pair.provider)
        .visit(
            [&](Consumer) { EXPECT(false); },
            [&](Binding::Failure failure) {
              EXPECT(failure == Binding::Failure::Unknown);
            });
  }
}

// Direct support on both sides terminates the greedy search. No data is copied
// and neither endpoint is asked about another protocol after that success.
VALIDATION_TEST(TtxFlow, direct_preference) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = 15,
    .values =
        {
          10,
          20,
          30,
          40,
        },
  };
  Validation::FlowTests::Reader reader = Validation::FlowTests::Reader(four);

  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);

  EXPECT(flow.get_protocol() == Protocol::Direct);
  EXPECT_EQ(writer.binds[0], Count(1));
  for (Count i = 1; i < 4; ++i) {
    EXPECT_EQ(writer.binds[i], Count(0));
    EXPECT_EQ(reader.binds[i], Count(0));
  }

  EXPECT_EQ(writer.commits, Count(0));
  EXPECT_EQ(writer.acquires, Count(0));
}

// Preference is not inheritance. A writer supplying only Direct cannot serve
// a reader accepting only Fragment, even though native code could read and
// manufacture a Fragment adapter. Flow must not invent that implementation.
VALIDATION_TEST(TtxFlow, disjoint_protocols) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_DIRECT,
  };
  Validation::FlowTests::Reader reader =
      Validation::FlowTests::Reader(four, PROVIDES_FRAGMENT);

  Flow flow;
  EXPECT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Unknown);

  for (Count i = 0; i < 4; ++i) {
    EXPECT_EQ(reader.binds[i], Count(1));
  }

  EXPECT_EQ(writer.binds[3], Count(1));
  EXPECT_EQ(writer.reads, Count(0));
}

// Identity acceptance is followed by one ABI agreement. A U32 reader cannot
// cast or copy an R32 representation merely because their byte widths match.
// Unknown tries the remaining candidates, while explicit rejection stops.
VALIDATION_TEST(TtxFlow, negotiation_failures) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = 15,
  };
  const auto wrong = prepare(Schema::range(real, 4, 4, 16, 4));
  Validation::FlowTests::Reader reader = Validation::FlowTests::Reader(wrong);

  Flow flow;
  EXPECT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Incompatible);
  EXPECT_EQ(writer.binds[3], Count(1));
  EXPECT_EQ(writer.acquires, Count(0));

  Validation::FlowTests::Reader undetermined =
      Validation::FlowTests::Reader(four, 0);
  undetermined.decline = Binding::Status::Unknown;
  flow.close();
  EXPECT(
      flow.connect(undetermined.query(), module.writer(writer)) ==
      Flow::Status::Unknown);
  for (Count index = 0; index != 4; ++index) {
    EXPECT_EQ(undetermined.binds[index], Count(1));
  }

  undetermined.decline = Binding::Status::Rejected;
  flow.close();
  EXPECT(
      flow.connect(undetermined.query(), module.writer(writer)) ==
      Flow::Status::Rejected);
  EXPECT_EQ(undetermined.binds[0], Count(2));
  for (Count index = 1; index != 4; ++index) {
    EXPECT_EQ(undetermined.binds[index], Count(1));
  }
}

// Exhaust the four protocol cooperation matrix using independent C and C++
// implementations. The expected answer is the first common advertised bit,
// not any capability that could be synthesized from a stronger protocol.
VALIDATION_TEST(TtxFlow, protocol_matrix) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  const Static::Vector<Protocol, 4> protocols = {
    {
      Protocol::Direct,
      Protocol::Shared,
      Protocol::Block,
      Protocol::Fragment,
    },
  };
  for (U8 source = 0; source < 16; ++source) {
    for (U8 target = 0; target < 16; ++target) {
      Module::State writer = {
        .provides = source,
      };
      Validation::FlowTests::Reader reader =
          Validation::FlowTests::Reader(four, target);

      Flow flow;
      const auto status = flow.connect(reader.query(), module.writer(writer));

      const U8 common = source & target;
      if (!common) {
        EXPECT(status == Flow::Status::Unknown);
        continue;
      }

      ASSERT(status == Flow::Status::Success);

      Count choice = 0;
      while (!(common & (1 << choice))) {
        ++choice;
      }

      EXPECT(flow.get_protocol() == protocols[choice]);
      for (Count i = choice + 1; i < 4; ++i) {
        EXPECT_EQ(reader.binds[i], Count(0));
        EXPECT_EQ(writer.binds[i], Count(0));
      }

      flow.close();
      EXPECT_EQ(writer.releases, choice == 1 ? Count(1) : Count(0));
    }
  }
}

// Binding a protocol is not yet agreement on its payload ABI. A reader may
// accept different representations under different protocols. An incompatible
// candidate performs no data access, allowing a later common ABI to win.
VALIDATION_TEST(TtxFlow, protocol_abi_match) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_DIRECT | PROVIDES_BLOCK,
  };
  const auto wrong = prepare(Schema::range(real, 4, 4, 16, 4));
  Validation::FlowTests::Reader reader = Validation::FlowTests::Reader(four);
  reader.direct_schema = &wrong;

  Flow flow;
  EXPECT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);
  EXPECT(flow.get_protocol() == Protocol::Block);
  EXPECT_EQ(writer.commits, Count(0));
}

// Each bind supplies its own answer. A previous successful publication must
// not make a later empty answer usable. This is the Query admission boundary,
// so Flow and operations can consume admitted handles without repeating it.
VALIDATION_TEST(TtxFlow, fresh_bind_results) {
  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_DIRECT,
  };
  auto published = module.writer(writer);

  struct Owner {
    Query query;
    Count calls = 0;
  } owner{
    published,
  };

  const Query query(ttx_semantic_query(
      &owner,
      [](const void* source, perimortem_uuid id,
         ttx_storage answer) -> ttx_binding_status {
        auto& owner = *const_cast<Owner*>(static_cast<const Owner*>(source));
        if (owner.calls++) {
          return TTX_BINDING_SATISFIED;
        }

        const auto native = static_cast<ttx_semantic_query>(owner.query);
        return native.bind(native.source, id, answer);
      },
      [](const void* source, perimortem_uuid id) -> ttx_binding_status {
        return static_cast<ttx_binding_status>(
            static_cast<const Owner*>(source)->query.supports(
                Perimortem::System::Uuid(id)));
      }));

  query
      .bind<Ttx::Data::Protocol::Direct::Provider>(
          Ttx::Semantic::Transport::Flow::direct.provider)
      .visit([&](auto) {}, [&](Binding::Failure) { EXPECT(false); });

  query
      .bind<Ttx::Data::Protocol::Direct::Provider>(
          Ttx::Semantic::Transport::Flow::direct.provider)
      .visit(
          [&](auto) { EXPECT(false); },
          [&](Binding::Failure status) {
            EXPECT(status == Binding::Failure::Rejected);
          });
}
