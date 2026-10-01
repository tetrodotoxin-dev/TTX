// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include <new>
#include <stdlib.h>

#include "perimortem/core/static/vector.hpp"

#include "perimortem/memory/dynamic/vector.hpp"

#include "toolchain/validation/benchmark.hpp"
#include "validation/unit_tests/semantic/fixtures.hpp"
#include "validation/unit_tests/semantic/measurement.hpp"
#include "ttx/semantic/transport/flow.hpp"

using namespace Validation::FlowTests;

// Establishment needs only a reader schema. Each synchronous Copy then takes
// its own Storage and returns a finished result without a retained request
// object.
VALIDATION_TEST(TtxFlow, copy_multiple_targets) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_DIRECT,
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

  Static::Vector<U32, 4> a, b;
  Measurement measurement;
  const auto first = Copy::flow(flow, storage(four, a));
  const auto second = Copy::flow(flow, storage(four, b));
  measurement.stop();

  EXPECT(first == Status::Success);
  EXPECT(second == Status::Success);
  EXPECT_EQ(a[3], U32(40));
  EXPECT_EQ(b[0], U32(10));

  EXPECT_EQ(reader.descriptions, Count(1));
  EXPECT_EQ(writer.descriptions, Count(1));
  EXPECT_EQ(writer.binds[0], Count(1));

  EXPECT_EQ(measurement.get_allocations(), Count(0));
  if (const auto copies = measurement.get_copies()) {
    EXPECT_EQ(*copies, Count(2));
  }
}

// Storage checks capacity and Copy checks agreement with the selected Flow.
// Neither failure reaches the provider or writes to the mismatching
// destination.
VALIDATION_TEST(TtxFlow, copy_target_mismatch) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_BLOCK,
  };
  Validation::FlowTests::Reader reader = Validation::FlowTests::Reader(four);

  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);

  U32 small = 99;
  Storage::create(
      four,
      {
        reinterpret_cast<U8*>(&small),
        sizeof(small),
      })
      .visit(
          [&](Storage) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Bounds); });

  EXPECT(
      Copy::flow(flow, storage(prepare(integer), small)) ==
      Status::Incompatible);
  EXPECT_EQ(small, U32(99));
  EXPECT_EQ(writer.commits, Count(0));
}

// A Storage can lend its reader without becoming the permanent destination. The
// agreement borrows only the schema, so another Storage receives this call's
// result.
VALIDATION_TEST(TtxFlow, reusable_reader) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_DIRECT,
    .values =
        {
          1,
          2,
          3,
          4,
        },
  };
  Static::Vector<U32, 4> original =
                             {
                               {
                                 99,
                                 99,
                                 99,
                                 99,
                               },
                             },
                         output = {};

  Flow flow;
  ASSERT(
      flow.connect(
          Flow::consumer(storage(four, original)), module.writer(writer)) ==
      Flow::Status::Success);

  EXPECT(Copy::flow(flow, storage(four, output)) == Status::Success);
  EXPECT_EQ(original[0], U32(99));
  EXPECT_EQ(output[3], U32(4));
}

// The C entry reports the same outcome as the native facade. The partial write
// below is observable because this test owns the failing provider, but Copy
// does not certify that prefix as a second kind of successful observation.
VALIDATION_TEST(TtxFlow, c_copy_result) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_FRAGMENT,
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

  Static::Vector<U32, 4> output;
  const auto outcome =
      ttx_copy(flow.get_abi(), storage(four, output).get_abi());
  EXPECT_EQ(outcome, TTX_DATA_SUCCESS);
  EXPECT_EQ(output[3], U32(4));

  writer.failure = 1;
  writer.fail_at = 3;
  writer.reads = 0;
  output[2] = 99;
  const auto failure =
      ttx_copy(flow.get_abi(), storage(four, output).get_abi());
  EXPECT_EQ(failure, TTX_DATA_IO_ERROR);
  EXPECT_EQ(output[2], U32(99));
}

// A zero count is useful only when the observer can see real work. Exercise
// the C, C++ and Bibliotheca routes, including nesting and an explicitly
// stopped scope. Optimization barriers keep the allocations observable to Clang.
VALIDATION_TEST(TtxFlow, measurement_routes) {
  Measurement outer;
  void* c = malloc(64);
  void* object = ::operator new(64);
  void* array = ::operator new[](64);
  void* aligned = ::operator new(64, std::align_val_t(64));
  void* aligned_array = ::operator new[](64, std::align_val_t(64));
  Toolchain::Validation::Benchmark::prevent_optimization(c);
  Toolchain::Validation::Benchmark::prevent_optimization(object);
  Toolchain::Validation::Benchmark::prevent_optimization(array);
  Toolchain::Validation::Benchmark::prevent_optimization(aligned);
  Toolchain::Validation::Benchmark::prevent_optimization(aligned_array);

  Measurement inner;
  Perimortem::Memory::Dynamic::Vector<U8> values;
  values.resize(1024);
  inner.stop();
  outer.stop();
  EXPECT_EQ(inner.get_allocations(), Count(1));
  EXPECT_EQ(
      outer.get_allocations(),
      Measurement::wraps_runtime() ? Count(6) : Count(1));

  values.resize(4096);
  EXPECT_EQ(inner.get_allocations(), Count(1));
  EXPECT_EQ(
      outer.get_allocations(),
      Measurement::wraps_runtime() ? Count(6) : Count(1));
  free(c);
  ::operator delete(object);
  ::operator delete[](array);
  ::operator delete(aligned, std::align_val_t(64));
  ::operator delete[](aligned_array, std::align_val_t(64));
}
