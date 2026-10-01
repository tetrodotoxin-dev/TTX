// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/static/vector.hpp"

#include "validation/unit_tests/semantic/fixtures.hpp"

using namespace Validation::FlowTests;

// Every commit borrows the exact Storage for that call, including capacity
// beyond its payload. The provider writes only the agreed record. The first
// result remains readable when a later call supplies another destination.
VALIDATION_TEST(TtxFlow, block_destinations) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_BLOCK,
    .values =
        {
          1,
          2,
          3,
          4,
        },
  };
  Validation::FlowTests::Reader reader = Validation::FlowTests::Reader(four);

  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);
  EXPECT_EQ(writer.commits, Count(0));

  Static::Vector<U32, 5> a, b;
  a[4] = 99;
  b[4] = 71;
  ASSERT(Copy::flow(flow, storage(four, a)) == Status::Success);

  writer.values[0] = 7;
  ASSERT(Copy::flow(flow, storage(four, b)) == Status::Success);
  EXPECT_EQ(a[0], U32(1));
  EXPECT_EQ(b[0], U32(7));

  EXPECT_EQ(a[4], U32(99));
  EXPECT_EQ(b[4], U32(71));
  EXPECT(writer.destination_representation == &four);
  EXPECT_EQ(writer.destination_capacity, sizeof(b));
  EXPECT_EQ(writer.commits, Count(2));
}

// A failed whole commit reports its cause. This fixture declines before
// writing, so its sentinel remains intact. That behavior belongs to this
// provider rather than a Copy promise to restore bytes on failure.
VALIDATION_TEST(TtxFlow, block_failure) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_BLOCK,
    .failure = 1,
  };
  Validation::FlowTests::Reader reader = Validation::FlowTests::Reader(four);

  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.writer(writer)) ==
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
  EXPECT_EQ(output[0], U32(99));
  EXPECT_EQ(writer.commits, Count(1));
}
