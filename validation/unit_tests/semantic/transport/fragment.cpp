// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/semantic/fixtures.hpp"

#include "perimortem/core/static/vector.hpp"

using namespace Validation::FlowTests;

// Copy promises an observation of the whole representation. A failure after
// two writes interrupts that observation without rolling the writes back. The
// result reports only the cause, leaving recovery to a policy that provides
// its own guarantees instead of treating a prefix as another successful result.
VALIDATION_TEST(TtxFlow, fragment_failure) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State provider = {
    .provides = PROVIDES_FRAGMENT,
    .failure = 1,
    .fail_at = 3,
    .values =
        {
          10,
          20,
          30,
          40,
        },
  };
  Validation::FlowTests::Consumer consumer =
      Validation::FlowTests::Consumer(four);

  Flow flow;
  ASSERT(
      flow.connect(consumer.query(), module.provider(provider)) ==
      Flow::Status::Success);

  Static::Vector<U32, 4> output = {
    {
      99,
      99,
      99,
      99,
    },
  };
  EXPECT(Copy::flow(flow, storage(four, output)) == Status::IoError);
  EXPECT_EQ(output[0], U32(10));
  EXPECT_EQ(output[1], U32(20));
  EXPECT_EQ(output[2], U32(99));

  EXPECT_EQ(provider.reads, Count(3));
}

// A failed read has no callback waiting to resume. Clearing this fixture's
// failure policy permits a new, ordinary copy using the same established Flow.
VALIDATION_TEST(TtxFlow, fragment_retry) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State provider = {
    .provides = PROVIDES_FRAGMENT,
    .failure = 1,
    .fail_at = 1,
    .values =
        {
          10,
          20,
          30,
          40,
        },
  };
  Validation::FlowTests::Consumer consumer =
      Validation::FlowTests::Consumer(four);

  Flow flow;
  ASSERT(
      flow.connect(consumer.query(), module.provider(provider)) ==
      Flow::Status::Success);

  Static::Vector<U32, 4> output;
  EXPECT(Copy::flow(flow, storage(four, output)) == Status::IoError);

  provider.failure = 0;
  EXPECT(Copy::flow(flow, storage(four, output)) == Status::Success);
  EXPECT_EQ(output[3], U32(40));

  EXPECT_EQ(provider.binds[3], Count(1));
}
