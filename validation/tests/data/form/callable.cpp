// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/tests/data/form/callable.h"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/storage/vector.hpp"

#include "ttx/data/encoding/callable.hpp"
#include "ttx/data/form/compiled.hpp"
#include "validation/tests/data/form/preparation.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Data;

static Harness Callables = {
  .name = "TTX::Data::Form::Callable",
};

static constexpr auto integer =
    Form::Schema::primitive(Form::Schema::Value::U32);
static constexpr auto real = Form::Schema::primitive(Form::Schema::Value::R64);

static constexpr Form::Schema::Argument four(integer, 4);
static constexpr auto function = Form::Schema::callable(
    Form::Callable::Convention::SystemVAMD64,
    {four, 1},
    {},
    8);
static constexpr auto& constant =
    Form::Compiled<function, 8>::get_representation();
static_assert(constant.get_bytes().get_size() == 16);

// Independently written words pin the selector positions, void sentinel and
// count compression. The only payload is the pointer at offset zero.
VALIDATION_TEST(Callables, canonical_words) {
  Validation::DataTests::Preparation prepare(8);
  const Storage::Vector<U8, 16> expected = {
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
      Form::Representation(expected.get_data(), sizeof(expected))));
  EXPECT(constant.compatible(prepare(function)));

  const Storage::Vector<Form::Schema::Argument, 4> singles = {
    {
      Form::Schema::Argument(integer, 1),
      {
        integer,
        1,
      },

      {
        integer,
        1,
      },

      {
        integer,
        1,
      },
    },
  };

  const auto expanded = Form::Schema::callable(
      Form::Callable::Convention::SystemVAMD64, singles.get_view().slice(0, 4),
      {}, 8);
  EXPECT(constant.compatible(prepare(expanded)));

  Count visited = 0;
  EXPECT(constant.visit([&](Form::Representation::Position position) {
    ++visited;
    EXPECT_EQ(position.get_offset(), Count(0));
    EXPECT(position.get_value() == Form::Schema::Value::Pointer);
    return Status::Success;
  }) == Status::Success);

  EXPECT_EQ(visited, Count(1));
}

// Argument order, convention and return type all affect the negotiated call.
// A function with no arguments remains a real pointer even when it returns
// void.
VALIDATION_TEST(Callables, signature_differences) {
  Validation::DataTests::Preparation prepare(8);
  const Storage::Vector<Form::Schema::Argument, 3> mixed = {
    {
      Form::Schema::Argument(integer, 1),
      {
        real,
        1,
      },

      {
        integer,
        1,
      },
    },
  };

  const Storage::Vector<Form::Schema::Argument, 2> ordered = {
    {
      Form::Schema::Argument(integer, 2),
      {
        real,
        1,
      },
    },
  };

  const auto a = Form::Schema::callable(
      Form::Callable::Convention::SystemVAMD64, mixed.get_view().slice(0, 3),
      {}, 8);
  const auto b = Form::Schema::callable(
      Form::Callable::Convention::SystemVAMD64, ordered.get_view().slice(0, 2),
      {}, 8);
  EXPECT_NOT(prepare(a).compatible(prepare(b)));

  const auto variadic = Form::Schema::callable(
      Form::Callable::Convention::SystemVAMD64Variadic, {four, 1}, {}, 8);
  const auto returned = Form::Schema::callable(
      Form::Callable::Convention::SystemVAMD64, {four, 1}, integer, 8);
  EXPECT_NOT(constant.compatible(prepare(variadic)));
  EXPECT_NOT(constant.compatible(prepare(returned)));

  // The same argument types can require different registers under System V
  // and Windows conventions. Canonical comparison therefore includes the
  // convention and distinguishes Windows fixed and variadic calls.
  const auto windows = Form::Schema::callable(
      Form::Callable::Convention::WindowsX64, {four, 1}, {}, 8);
  const auto windows_variadic = Form::Schema::callable(
      Form::Callable::Convention::WindowsX64Variadic, {four, 1}, {}, 8);
  EXPECT_NOT(constant.compatible(prepare(windows)));
  EXPECT_NOT(prepare(windows).compatible(prepare(windows_variadic)));

  const auto empty = Form::Schema::callable(
      Form::Callable::Convention::SystemVAMD64, {}, {}, 8);
  const auto& ready = prepare(empty);
  EXPECT_EQ(ready.get_extent(), Count(8));
  EXPECT_EQ(ready.get_bytes().get_size(), Count(16));

  const Form::Schema::Position field(empty, 0);
  EXPECT(ready.compatible(prepare(Form::Schema::composite({field, 1}, 8, 8))));
}

// A large formal count remains one run. The wider header's unused fields and
// the full width void sentinel are checked without expanding those parameters.
VALIDATION_TEST(Callables, wide_signature) {
  Validation::DataTests::Preparation prepare(8);
  const Form::Schema::Argument many(integer, 1000000000);
  const auto large = Form::Schema::callable(
      Form::Callable::Convention::SystemVAMD64, {many, 1}, {}, 8);
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
      prepare(Form::Schema::range(function, 2, distance, distance + 8, 8));
  EXPECT_EQ(extended.get_depth(), U8(9));

  const auto wide = extended.get_bytes();
  const Count wide_start = (Count(2) * 9) << 5;
  EXPECT_EQ(Encoding::Block::extract(wide, wide_start + 0, 64), Count(-1));
  EXPECT_EQ(Encoding::Block::extract(wide, wide_start + 64, 10), Count(1023));
  EXPECT_EQ(Encoding::Block::extract(wide, wide_start + 74, 64), Count(0));
  EXPECT_EQ(Encoding::Block::extract(wide, wide_start + 138, 6), Count(0));
}

VALIDATION_TEST(Callables, invalid_signatures) {
  Form::Compiler compiler;
  const auto array = Form::Schema::range(integer, 4, 4, 16, 4);
  const Form::Schema::Argument array_argument(array, 1);
  EXPECT(
      compiler.compile(
          Form::Schema::callable(
              Form::Callable::Convention::SystemVAMD64, {array_argument, 1}, {},
              8),
          8) == Status::Invalid);

  const Form::Schema::Argument missing(integer, 0);
  EXPECT(
      compiler.compile(
          Form::Schema::callable(
              Form::Callable::Convention::SystemVAMD64, {missing, 1}, {}, 8),
          8) == Status::Invalid);
  EXPECT(
      compiler.compile(
          Form::Schema::callable(Form::Callable::Convention(9), {}, {}, 8),
          8) == Status::Invalid);

  const Storage::Vector<Form::Schema::Argument, 2> overflow = {
    {
      Form::Schema::Argument(integer, Count(-1)),
      {
        integer,
        1,
      },
    },
  };

  EXPECT(
      compiler.compile(
          Form::Schema::callable(
              Form::Callable::Convention::SystemVAMD64,
              overflow.get_view().slice(0, overflow.get_size()), {}, 8),
          8) == Status::Overflow);

  const auto pointer = Form::Schema::pointer(&array);
  const Form::Schema::Argument indirect(pointer, 1);
  EXPECT(
      compiler.compile(
          Form::Schema::callable(
              Form::Callable::Convention::SystemVAMD64, {indirect, 1}, {}, 8),
          8) == Status::Success);
}

// C authors its descriptions and executable table independently. Agreement
// permits one ordinary copy into the C++ table, followed by actual fixed,
// variadic and SIMD calls. No Semantic negotiation or invocation wrapper is
// involved in this Data format proof.
VALIDATION_TEST(Callables, foreign_calls) {
  Validation::DataTests::Preparation prepare;
  const auto vector = Form::Schema::primitive(Form::Schema::Value::V128);
  const Form::Schema::Argument count(integer, 1), packed(vector, 1);
  const auto sum = Form::Schema::callable(
      Form::Callable::Convention::Native, {four, 1}, integer);
  const auto variadic = Form::Schema::callable(
      Form::Callable::Convention::NativeVariadic, {count, 1}, real);
  const auto twice = Form::Schema::callable(
      Form::Callable::Convention::Native, {packed, 1}, vector);
  const Storage::Vector<Form::Schema::Position, 3> fields = {
    {
      Form::Schema::Position(sum, __builtin_offsetof(ttx_test_callables, sum)),
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

  const auto table = Form::Schema::composite(
      fields.get_view().slice(0, 3), sizeof(ttx_test_callables),
      alignof(ttx_test_callables));
  ASSERT(prepare(table).compatible(prepare(*ttx_test_callable_schema())));

  ttx_test_callables output;
  Data::copy(
      Data::cast<U8>(&output), Data::cast<const U8>(ttx_test_callable_table()),
      sizeof(output));
  EXPECT_EQ(output.sum(1, 2, 3, 4), U32(10));
  EXPECT_EQ(output.variadic(3, 1.0, 2.0, 3.0), R64(6));

  const ttx_test_vector input = {1, 2, 3, 4};
  const auto doubled = output.twice(input);
  for (Count i = 0; i < 4; ++i) {
    EXPECT_EQ(doubled[i], input[i] * 2);
  }
}
