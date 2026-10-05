// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/semantic/fixtures.hpp"

#include "perimortem/core/static/vector.hpp"

using namespace Validation::FlowTests;

// Acquisition returns a ready lifetime. Flow retains it across calls, so Copy
// never reacquires it and closing releases the agreement exactly once.
VALIDATION_TEST(TtxFlow, shared_lifetime) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State provider = {
    .provides = PROVIDES_SHARED,
    .values =
        {
          1,
          2,
          3,
          4,
        },
  };
  Validation::FlowTests::Consumer consumer =
      Validation::FlowTests::Consumer(four);

  Flow flow;
  ASSERT(
      flow.connect(consumer.query(), module.provider(provider)) ==
      Flow::Status::Success);

  Static::Vector<U32, 4> a, b;
  EXPECT(Copy::flow(flow, storage(four, a)) == Status::Success);
  EXPECT(Copy::flow(flow, storage(four, b)) == Status::Success);

  EXPECT_EQ(provider.acquires, Count(1));
  EXPECT_EQ(provider.releases, Count(0));
  EXPECT_EQ(b[3], U32(4));

  flow.close();
  flow.close();
  EXPECT_EQ(provider.releases, Count(1));
}

// A selected Shared provider can fail acquisition. The returned failure ends
// that attempt instead of silently negotiating Block afterward.
VALIDATION_TEST(TtxFlow, shared_failure) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State provider = {
    .provides = PROVIDES_SHARED | PROVIDES_BLOCK,
    .failure = 1,
  };
  Validation::FlowTests::Consumer consumer =
      Validation::FlowTests::Consumer(four);

  Flow flow;
  EXPECT(
      flow.connect(consumer.query(), module.provider(provider)) ==
      Flow::Status::IoError);
  EXPECT_EQ(provider.binds[2], Count(0));
  EXPECT_EQ(provider.releases, Count(0));
}

// Release still invokes provider code even though acquisition and operations
// are synchronous. That hook must see the old Flow unpublished before
// reconnecting.
struct OnRelease {
  Flow& flow;
  Validation::FlowTests::Consumer& consumer;
  Query next;
  Bool was_closed = false;
  Flow::Status result = Flow::Status::Rejected;

  static void run(void* context) {
    auto& observer = *static_cast<OnRelease*>(context);
    observer.was_closed = observer.flow.get_protocol() == Protocol::None;
    observer.result =
        observer.flow.connect(observer.consumer.query(), observer.next);
  }
};

VALIDATION_TEST(TtxFlow, release_reentrancy) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State first = {
    .provides = PROVIDES_SHARED,
  };
  Module::State second = {
    .provides = PROVIDES_DIRECT,
    .values =
        {
          1,
          2,
          3,
          4,
        },
  };
  Validation::FlowTests::Consumer consumer =
      Validation::FlowTests::Consumer(four);
  Flow flow;

  OnRelease observer = OnRelease(flow, consumer, module.provider(second));
  first.context = &observer;
  first.released = OnRelease::run;

  ASSERT(
      flow.connect(consumer.query(), module.provider(first)) ==
      Flow::Status::Success);

  flow.close();
  EXPECT(observer.was_closed);
  EXPECT(observer.result == Flow::Status::Success);

  Static::Vector<U32, 4> output;
  EXPECT(Copy::flow(flow, storage(four, output)) == Status::Success);
  EXPECT_EQ(output[3], U32(4));
}
