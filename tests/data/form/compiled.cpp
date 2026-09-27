// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/compiled.hpp"

#include "perimortem/core/static/vector.hpp"
#include "perimortem/core/null_terminated.hpp"

#include "tests/data/form/preparation.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static Toolchain::Validation::Harness TtxCompiled = {
  .name = "TTX::Data::Form::Compiled",
};
static constexpr auto integer = Schema::primitive(Schema::Value::U32);
static constexpr auto real = Schema::primitive(Schema::Value::R32);
static constexpr auto billion =
    Schema::range(integer, 1000000000, 4, 4000000000, 4);
static constexpr auto& prepared = Compiled<billion>::get_representation();

// The count changes field width, not the size of the temporary inventory.
// Even one billion values need only a header and one repeated element block.
static_assert(prepared.get_extent() == 4000000000);
static_assert(prepared.get_depth() == 4);
static_assert(prepared.get_bytes().get_size() == 32);

static constexpr auto fields = [] {
  Static::Vector<Schema::Position, 512> result;
  for (Count i = 0; i < result.get_size(); ++i) {
    result[i] = Schema::Position(i % 2 ? integer : real, i * 4);
  }

  return result;
}();
static constexpr auto record = Schema::composite(fields.get_view(), 2048, 4);
static constexpr auto& compiled_record = Compiled<record>::get_representation();
static_assert(compiled_record.get_extent() == 2048);
static_assert(compiled_record.get_bytes().get_size() == 513 * 8);

static constexpr auto empty =
    Schema::composite(View::Vector<Schema::Position>(), 0);
static constexpr auto& compiled_empty = Compiled<empty>::get_representation();
static_assert(compiled_empty.get_extent() == 0);
static_assert(compiled_empty.get_bytes().get_size() == 8);

VALIDATION_TEST(TtxCompiled, runtime_agreement) {
  Validation::DataTests::Preparation prepare;

  EXPECT(prepared.compatible(prepare(billion)));
  EXPECT(compiled_record.compatible(prepare(record)));
  EXPECT(compiled_empty.compatible(prepare(empty)));
}

VALIDATION_TEST(TtxCompiled, primitive_bytes) {
  // These bytes are written independently of Encoding's field helpers. They
  // establish the first byte depth, header geometry and primitive descriptor.
  // Header: F=1, C=1, alignment=4, extent=4. Element: N=1, O=0, D=4, U32=3.
  const Static::Vector<U8, 8> expected = {
    {
      0x11,
      0x40,
      0x40,
      0x00,
      0x03,
      0x10,
      0x00,
      0x01,
    },
  };
  const auto& representation = Compiled<integer>::get_representation();

  ASSERT_EQ(representation.get_bytes().get_size(), sizeof(expected));
  EXPECT(
      Data::compare(
          representation.data, expected.get_data(), sizeof(expected)));

  // A borrowed publication must also be usable for observation. This checks
  // the literal distance against the current format rather than merely
  // comparing two copies of the same byte array.
  Representation(expected.get_data(), sizeof(expected))
      .next(0)
      .visit(
          [&](Representation::Position position) {
            EXPECT_EQ(position.offset, Count(0));
            EXPECT(position.get_value() == Schema::Value::U32);
            EXPECT_EQ(position.get_extent(), Count(4));
          },
          [&](Status) { EXPECT(false); });
}

VALIDATION_TEST(TtxCompiled, repeated_publication) {
  // Preparing owns temporary records. Two writes after preparation must not
  // depend on the Schema still being present, or compile it a second time.
  Compiler compiler;
  {
    auto source = integer;
    ASSERT(compiler.compile(source) == Status::Success);
    source.data.value.type = TTX_SCHEMA_R64;
  }

  Static::Vector<U8, 8> first;
  Static::Vector<U8, 8> second;
  ASSERT_EQ(compiler.get_size(), Count(8));
  ASSERT(
      compiler.write(Access::Bytes(first.get_data(), first.get_size())) ==
      Status::Success);
  ASSERT(
      compiler.write(Access::Bytes(second.get_data(), second.get_size())) ==
      Status::Success);

  const auto& reference = Compiled<integer>::get_representation();
  EXPECT(Representation(first.get_data(), sizeof(first)).compatible(reference));
  EXPECT(Data::compare(first.get_data(), second.get_data(), sizeof(first)));
}

// Padding belongs to the complete publication, not each descriptor. F1 and
// F3 both leave four tail bytes after these three blocks. Empty has only
// its root block. Dirty destination bytes make an unwritten tail observable.
VALIDATION_TEST(TtxCompiled, canonical_padding) {
  const Static::Vector<Schema::Position, 2> small_fields = {
    {
      Schema::Position{
        integer,
        0,
      },
      {
        real,
        4,
      },
    },
  };
  const Static::Vector<Schema::Position, 2> wide_fields = {
    {
      Schema::Position{
        integer,
        0,
      },
      {
        real,
        65536,
      },
    },
  };
  const Static::Vector<Schema, 3> forms = {
    {
      empty,
      Schema::composite(View::Vector<Schema::Position>(small_fields), 8, 4),
      Schema::composite(View::Vector<Schema::Position>(wide_fields), 65540, 4),
    },
  };
  const Static::Vector<Count, 3> ends = {
    {
      4,
      12,
      36,
    },
  };
  Static::Vector<U8, 48> buffer;
  Compiler compiler;
  for (Count form = 0; form < 3; ++form) {
    ASSERT(compiler.compile(forms[form]) == Status::Success);
    const Count size = compiler.get_size();
    ASSERT_EQ(size, ends[form] + 4);
    for (Count i = 0; i < buffer.get_size(); ++i) {
      buffer[i] = 0xa5;
    }

    EXPECT(
        compiler.write(Access::Bytes(buffer.get_data(), size - 1)) ==
        Status::Bounds);
    for (const auto byte : buffer.get_view()) {
      ASSERT_EQ(byte, U8(0xa5));
    }

    ASSERT(
        compiler.write(Access::Bytes(buffer.get_data(), size)) ==
        Status::Success);
    for (Count i = ends[form]; i < size; ++i) {
      EXPECT_EQ(buffer[i], U8(0));
    }

    for (Count i = size; i < sizeof(buffer); ++i) {
      EXPECT_EQ(buffer[i], U8(0xa5));
    }
  }

  const Static::Vector<U8, 8> expected = {
    {
      0x01,
      0x10,
      0,
      0,
      0,
      0,
      0,
      0,
    },
  };
  EXPECT(compiled_empty.compatible(
      Representation(expected.get_data(), sizeof(expected))));
}
