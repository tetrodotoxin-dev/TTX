// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/data/form/callable.h"

#include "perimortem/core/static/vector.hpp"

#include "validation/unit_tests/data/form/preparation.hpp"
#include "toolchain/validation/unit_test.hpp"
#include "ttx/data/encoding/callable.hpp"
#include "ttx/data/form/compiled.hpp"

using namespace Perimortem;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static Toolchain::Validation::Harness Callables = {
  .name = "TTX::Data::Form::Callable",
};
static constexpr auto integer = Schema::primitive(Schema::Value::U32);
static constexpr auto real = Schema::primitive(Schema::Value::R64);
static constexpr Schema::Argument four(integer, 4);
static constexpr auto function = Schema::callable(
    Schema::Convention::SystemVAMD64,
    {
      &four,
      1,
    });
static constexpr auto& constant = Compiled<function>::get_representation();
static_assert(constant.get_bytes().get_size() == 16);

// Independently written words pin the selector positions, void sentinel and
// count compression. The only payload is the pointer at offset zero.
VALIDATION_TEST(Callables, canonical_words) {
  Validation::DataTests::Preparation prepare;
  const Perimortem::Core::Static::Vector<U8, 16> expected = {
    {
      0x11,
      0x80,
      0x80,
      0x00,
      0x82,
      0x21,
      0x00,
      0x01,
      0xff,
      0x03,
      0x01,
      0x04,
      0x03,
      0x00,
      0x00,
      0x04,
    },
  };
  EXPECT(constant.compatible(
      Representation(expected.get_data(), sizeof(expected))));
  EXPECT(constant.compatible(prepare(function)));

  const Perimortem::Core::Static::Vector<Schema::Argument, 4> singles = {
    {
      Schema::Argument(integer),
      {
        integer,
      },
      {
        integer,
      },
      {
        integer,
      },
    },
  };
  const auto expanded = Schema::callable(
      Schema::Convention::SystemVAMD64, {
                                          singles.get_data(),
                                          4,
                                        });
  EXPECT(constant.compatible(prepare(expanded)));
  Count visited = 0;
  EXPECT(constant.visit([&](Representation::Position position) {
    ++visited;
    EXPECT_EQ(position.offset, Count(0));
    EXPECT(position.get_value() == Schema::Value::Pointer);
    return Status::Success;
  }) == Status::Success);
  EXPECT_EQ(visited, Count(1));
}

// Argument order, convention and return type all affect the negotiated call.
// A function with no arguments remains a real pointer even when it returns
// void.
VALIDATION_TEST(Callables, signature_differences) {
  Validation::DataTests::Preparation prepare;
  const Perimortem::Core::Static::Vector<Schema::Argument, 3> mixed = {
    {
      Schema::Argument(integer),
      {
        real,
      },
      {
        integer,
      },
    },
  };
  const Perimortem::Core::Static::Vector<Schema::Argument, 2> ordered = {
    {
      Schema::Argument(integer, 2),
      {
        real,
      },
    },
  };
  const auto a = Schema::callable(
      Schema::Convention::SystemVAMD64, {
                                          mixed.get_data(),
                                          3,
                                        });
  const auto b = Schema::callable(
      Schema::Convention::SystemVAMD64, {
                                          ordered.get_data(),
                                          2,
                                        });
  EXPECT_NOT(prepare(a).compatible(prepare(b)));
  const auto variadic = Schema::callable(
      Schema::Convention::SystemVAMD64Variadic, {
                                                  &four,
                                                  1,
                                                });
  const auto returned = Schema::callable(
      Schema::Convention::SystemVAMD64,
      {
        &four,
        1,
      },
      &integer);
  EXPECT_NOT(constant.compatible(prepare(variadic)));
  EXPECT_NOT(constant.compatible(prepare(returned)));

  // Equal fields do not make System V and Windows call boundaries agree.
  // Windows fixed and variadic declarations also remain distinct promises.
  const auto windows = Schema::callable(
      Schema::Convention::WindowsX64, {
                                        &four,
                                        1,
                                      });
  const auto windows_variadic = Schema::callable(
      Schema::Convention::WindowsX64Variadic, {
                                                &four,
                                                1,
                                              });
  EXPECT_NOT(constant.compatible(prepare(windows)));
  EXPECT_NOT(prepare(windows).compatible(prepare(windows_variadic)));

  const auto empty = Schema::callable(Schema::Convention::SystemVAMD64, {});
  const auto& ready = prepare(empty);
  EXPECT_EQ(ready.get_extent(), Count(8));
  EXPECT_EQ(ready.get_bytes().get_size(), Count(16));
  const Schema::Position field(empty, 0);
  EXPECT(ready.compatible(prepare(Schema::composite({&field, 1}, 8, 8))));
}

// A large formal count remains one run. The wider header's unused fields and
// the full width void sentinel are checked without expanding those parameters.
VALIDATION_TEST(Callables, wide_signature) {
  Validation::DataTests::Preparation prepare;
  const Schema::Argument many(integer, 1000000000);
  const auto large = Schema::callable(
      Schema::Convention::SystemVAMD64, {
                                          &many,
                                          1,
                                        });
  const auto& ready = prepare(large);
  EXPECT_EQ(ready.get_depth(), U8(4));
  EXPECT_EQ(ready.get_bytes().get_size(), Count(64));
  const auto block = ready.get_bytes();
  const Count start = (Count(2) * 4) << 5;
  EXPECT_EQ(Encoding::Block::extract(block, start + 34, 30), Count(0));
  EXPECT_EQ(
      Encoding::Block::extract(block, start + 0, 34), (Count(1) << 34) - 1);
  const auto header = Encoding::Callable::decode(block, 2, 4);
  EXPECT_EQ(header.count, Count(1000000000));
  EXPECT(header.returns_void);

  // A distance above 63 bits selects F9. The void marker spans two U64
  // chunks here, while every reserved bit above it must remain zero.
  const Count distance = Count(1) << 63;
  const auto& extended =
      prepare(Schema::range(function, 2, distance, distance + 8, 8));
  EXPECT_EQ(extended.get_depth(), U8(9));
  const auto wide = extended.get_bytes();
  const Count wide_start = (Count(2) * 9) << 5;
  EXPECT_EQ(Encoding::Block::extract(wide, wide_start + 0, 64), Count(-1));
  EXPECT_EQ(Encoding::Block::extract(wide, wide_start + 64, 10), Count(1023));
  EXPECT_EQ(Encoding::Block::extract(wide, wide_start + 74, 64), Count(0));
  EXPECT_EQ(Encoding::Block::extract(wide, wide_start + 138, 6), Count(0));
}

VALIDATION_TEST(Callables, invalid_signatures) {
  Compiler compiler;
  const auto array = Schema::range(integer, 4, 4, 16, 4);
  const Schema::Argument array_argument(array);
  EXPECT(
      compiler.compile(
          Schema::callable(
              Schema::Convention::SystemVAMD64, {&array_argument, 1})) ==
      Status::Invalid);
  const Schema::Argument missing(integer, 0);
  EXPECT(
      compiler.compile(
          Schema::callable(Schema::Convention::SystemVAMD64, {&missing, 1})) ==
      Status::Invalid);
  EXPECT(
      compiler.compile(Schema::callable(Schema::Convention(9), {})) ==
      Status::Invalid);
  const Perimortem::Core::Static::Vector<Schema::Argument, 2> overflow = {
    {
      Schema::Argument(integer, Count(-1)),
      {
        integer,
      },
    },
  };
  EXPECT(
      compiler.compile(
          Schema::callable(
              Schema::Convention::SystemVAMD64, {
                                                  overflow.get_data(),
                                                  overflow.get_size(),
                                                })) == Status::Overflow);
  const auto pointer = Schema::pointer(&array);
  const Schema::Argument indirect(pointer);
  EXPECT(
      compiler.compile(
          Schema::callable(Schema::Convention::SystemVAMD64, {&indirect, 1})) ==
      Status::Success);
}

// C authors its descriptions and executable table independently. Agreement
// admits one ordinary copy into the C++ table, followed by actual fixed,
// variadic and SIMD calls. No Semantic negotiation or invocation wrapper is
// involved in this Data format proof.
VALIDATION_TEST(Callables, foreign_calls) {
  Validation::DataTests::Preparation prepare;
  const auto vector = Schema::primitive(Schema::Value::V128);
  const Schema::Argument count(integer), packed(vector);
  const auto sum = Schema::callable(
      Schema::Convention::SystemVAMD64,
      {
        &four,
        1,
      },
      &integer);
  const auto variadic = Schema::callable(
      Schema::Convention::SystemVAMD64Variadic,
      {
        &count,
        1,
      },
      &real);
  const auto twice = Schema::callable(
      Schema::Convention::SystemVAMD64,
      {
        &packed,
        1,
      },
      &vector);
  const Perimortem::Core::Static::Vector<Schema::Position, 3> fields = {
    {
      Schema::Position(sum, __builtin_offsetof(ttx_test_callables, sum)),
      {
        variadic,
        __builtin_offsetof(ttx_test_callables, variadic),
      },
      {
        twice,
        __builtin_offsetof(ttx_test_callables, twice),
      },
    },
  };
  const auto table = Schema::composite(
      {
        fields.get_data(),
        3,
      },
      sizeof(ttx_test_callables), alignof(ttx_test_callables));
  ASSERT(prepare(table).compatible(prepare(*ttx_test_callable_schema())));

  ttx_test_callables output;
  Core::Data::copy(
      reinterpret_cast<U8*>(&output),
      static_cast<const U8*>(ttx_test_callable_table()), sizeof(output));
  EXPECT_EQ(output.sum(1, 2, 3, 4), U32(10));
  EXPECT_EQ(output.variadic(3, 1.0, 2.0, 3.0), R64(6));
  const ttx_test_vector input = {1, 2, 3, 4};
  const auto doubled = output.twice(input);
  for (Count i = 0; i < 4; ++i) {
    EXPECT_EQ(doubled[i], input[i] * 2);
  }
}
