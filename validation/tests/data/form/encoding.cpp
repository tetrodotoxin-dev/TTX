// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/data.hpp"
#include "perimortem/core/storage/vector.hpp"

#include "ttx/data/form/compiled.hpp"
#include "validation/tests/data/form/preparation.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Data;
using namespace Validation::DataTests;

static Harness TtxEncoding = {
  .name = "TTX::Data::Form::Encoding",
};

static constexpr auto byte = Form::Schema::primitive(Form::Schema::Value::U8);
static constexpr auto integer =
    Form::Schema::primitive(Form::Schema::Value::U32);

// These are literal streams, not bytes generated with Encoding's helpers.
// Crossing each array threshold exercises a new global field width. F3 also
// proves that its trailing U32 follows the lower U64 without padding.
VALIDATION_TEST(TtxEncoding, depth_bytes) {
  Preparation prepare;
  const Storage::Vector<U8, 16> f2 = {
    {
      0x12,
      0x00,
      0x10,
      0x00,
      0x00,
      0x10,
      0x00,
      0x00,
      0x01,
      0x00,
      0x04,
      0x00,
      0x00,
      0x00,
      0x00,
      0x01,
    },
  };

  const Storage::Vector<U8, 24> f3 = {
    {
      0x13, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    },
  };

  const Storage::Vector<U8, 32> f4 = {
    {
      0x14, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    },
  };

  EXPECT(prepare(Form::Schema::range(byte, 256, 1, 256))
             .compatible(Form::Representation(f2.get_data(), sizeof(f2))));
  EXPECT(prepare(Form::Schema::range(byte, 65536, 1, 65536))
             .compatible(Form::Representation(f3.get_data(), sizeof(f3))));
  EXPECT(prepare(Form::Schema::range(byte, 16777216, 1, 16777216))
             .compatible(Form::Representation(f4.get_data(), sizeof(f4))));
}

// Equal occupied bytes do not erase a real struct boundary. The flat case
// needs two blocks. The array of cells needs a referenced cell header and its
// primitive, even though both payloads occupy 24 bytes at alignment eight.
VALIDATION_TEST(TtxEncoding, struct_bytes) {
  Preparation prepare;
  const Storage::Vector<Form::Schema::Position, 3> fields = {
    {
      Form::Schema::Position(integer, 0),
      {
        integer,
        8,
      },

      {
        integer,
        16,
      },
    },
  };

  const auto flat = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(fields[0], 3), 24, 8);
  const Form::Schema::Position member(integer, 0);
  const auto cell = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(member, 1), 8, 8);
  const auto cells = Form::Schema::range(cell, 3, 8, 24, 8);
  const Storage::Vector<U8, 8> flat_bytes = {
    {
      0x11,
      0x80,
      0x80,
      0x01,
      0x03,
      0x20,
      0x00,
      0x03,
    },
  };

  const Storage::Vector<U8, 16> cell_bytes = {
    {
      0x11,
      0x80,
      0x80,
      0x01,
      0x02,
      0x22,
      0x00,
      0x03,
      0x11,
      0x80,
      0x80,
      0x00,
      0x03,
      0x10,
      0x00,
      0x01,
    },
  };

  EXPECT(prepare(flat).compatible(
      Form::Representation(flat_bytes.get_data(), sizeof(flat_bytes))));
  EXPECT(prepare(cells).compatible(
      Form::Representation(cell_bytes.get_data(), sizeof(cell_bytes))));
  EXPECT_NOT(prepare(flat).compatible(prepare(cells)));
}

// Consecutive bytes coalesce without swallowing the padding before U32.
// Both ordinary members and an authored byte array produce this same stream.
VALIDATION_TEST(TtxEncoding, padding_bytes) {
  Preparation prepare;
  const Storage::Vector<Form::Schema::Position, 4> fields = {
    {
      Form::Schema::Position(byte, 0),
      {
        byte,
        1,
      },

      {
        byte,
        2,
      },

      {
        integer,
        4,
      },
    },
  };

  const auto bytes = Form::Schema::range(byte, 3, 1, 3);
  const Storage::Vector<Form::Schema::Position, 2> grouped = {
    {
      Form::Schema::Position(bytes, 0),
      {
        integer,
        4,
      },
    },
  };

  const auto flat = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(fields[0], 4), 8, 4);
  const auto array = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(grouped[0], 2), 8, 4);
  const Storage::Vector<U8, 16> expected = {
    {
      0x21,
      0x40,
      0x80,
      0x00,
      0x01,
      0x04,
      0x00,
      0x03,
      0x03,
      0x10,
      0x04,
      0x01,
      0x00,
      0x00,
      0x00,
      0x00,
    },
  };

  EXPECT(prepare(flat).compatible(
      Form::Representation(expected.get_data(), sizeof(expected))));
  EXPECT(prepare(flat).compatible(prepare(array)));
}

VALIDATION_TEST(TtxEncoding, native_profiles) {
  Preparation prepare;
  for (U8 depth = 1; depth <= 8; ++depth) {
    const Count count = Count(1) << (8 * (depth - 1));
    const auto schema = Form::Schema::range(byte, count, 1, count);
    const auto& representation = prepare(schema);

    EXPECT_EQ(representation.get_depth(), depth);
    EXPECT_EQ(representation.get_extent(), count);
    EXPECT_EQ(representation.get_bytes().get_size(), Count(8 * depth));
  }
}

VALIDATION_TEST(TtxEncoding, field_promotions) {
  Preparation prepare;
  const Form::Schema::Position distant(byte, 256);
  const Form::Schema::Position first(byte, 0);
  const Storage::Vector<Form::Schema, 5> forms = {
    {
      Form::Schema::range(byte, 256, 1, 256),
      Form::Schema::composite(
          View::Vector<Form::Schema::Position>(distant, 1), 257),
      Form::Schema::range(byte, 2, 256, 257),
      Form::Schema::composite(
          View::Vector<Form::Schema::Position>(first, 1), 4096),
      Form::Schema::composite(
          View::Vector<Form::Schema::Position>(first, 1), 256, 256),
    },
  };

  for (Count schema_index = 0; schema_index < forms.get_size();
       ++schema_index) {
    const auto& schema = forms[schema_index];
    EXPECT_EQ(prepare(schema).get_depth(), U8(2));
  }

  // Alternating byte types prevent run compaction, forcing the header count
  // to widen while offsets and primitive codes still fit the narrow profile.
  const auto signed_byte = Form::Schema::primitive(Form::Schema::Value::S8);
  Storage::Vector<Form::Schema::Position, 256> fields;
  for (Count i = 0; i < 256; ++i) {
    fields[i] = Form::Schema::Position(i % 2 ? byte : signed_byte, i);
  }

  const auto wide = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(fields[0], 256), 256);
  EXPECT_EQ(prepare(wide).get_depth(), U8(2));
}

// Each wrapper is a real, distinct body. At 65 headers the deepest referenced
// body starts at block 128, exceeding narrow P even though all extents, counts
// and offsets still fit F1. The whole stream must promote together.
VALIDATION_TEST(TtxEncoding, reference_promotion) {
  Preparation prepare;
  Storage::Vector<Form::Schema, 65> bodies;
  Storage::Vector<Form::Schema::Position, 65> fields;
  for (Count i = 0; i < 65; ++i) {
    fields[i] = Form::Schema::Position(i ? &bodies[i - 1] : &byte, 0);
    bodies[i] = Form::Schema::composite(
        View::Vector<Form::Schema::Position>(fields[i], 1), 1);
  }

  EXPECT_EQ(prepare(bodies[63]).get_depth(), U8(1));
  EXPECT_EQ(prepare(bodies[64]).get_depth(), U8(2));
}

// The first member of the second input range extends the first output run,
// while its remainder starts a singleton. Treating source ranges as atomic
// descriptors would make these two equivalent inputs encode differently.
VALIDATION_TEST(TtxEncoding, split_run_prefix) {
  Preparation prepare;
  const auto first = Form::Schema::range(integer, 2, 4, 8, 4);
  const auto second = Form::Schema::range(integer, 2, 8, 12, 4);
  const Storage::Vector<Form::Schema::Position, 2> split = {
    {
      Form::Schema::Position(first, 0),
      {
        second,
        8,
      },
    },
  };

  const Storage::Vector<Form::Schema::Position, 4> flat = {
    {
      Form::Schema::Position(integer, 0),
      {
        integer,
        4,
      },

      {
        integer,
        8,
      },

      {
        integer,
        16,
      },
    },
  };

  const auto a = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(split[0], 2), 20, 4);
  const auto b = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(flat[0], 4), 20, 4);

  EXPECT(prepare(a).compatible(prepare(b)));
  EXPECT_EQ(prepare(a).get_bytes().get_size(), Count(16));
}

// Gaps between batches prevent a single arithmetic progression. The format
// needs three runs here, but still needs no synthetic struct to hold padding.
VALIDATION_TEST(TtxEncoding, gapped_batches) {
  Preparation prepare;
  const auto pair = Form::Schema::range(integer, 2, 4, 8, 4);
  const auto repeated = Form::Schema::range(pair, 3, 12, 32, 4);
  const Storage::Vector<Form::Schema::Position, 6> fields = {
    {
      Form::Schema::Position(integer, 0),
      {
        integer,
        4,
      },

      {
        integer,
        12,
      },

      {
        integer,
        16,
      },

      {
        integer,
        24,
      },

      {
        integer,
        28,
      },
    },
  };

  const auto flat = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(fields[0], 6), 32, 4);

  EXPECT(prepare(repeated).compatible(prepare(flat)));
  EXPECT_EQ(prepare(repeated).get_bytes().get_size(), Count(16));
}

VALIDATION_TEST(TtxEncoding, interleaved_runs) {
  Preparation prepare;
  const auto signed_byte = Form::Schema::primitive(Form::Schema::Value::S8);
  const auto even = Form::Schema::range(byte, 3, 4, 9);
  const auto odd = Form::Schema::range(signed_byte, 2, 4, 5);
  const Storage::Vector<Form::Schema::Position, 2> ranges = {
    {
      Form::Schema::Position(even, 0),
      {
        odd,
        2,
      },
    },
  };

  const Storage::Vector<Form::Schema::Position, 5> fields = {
    {
      Form::Schema::Position(byte, 0),
      {
        signed_byte,
        2,
      },

      {
        byte,
        4,
      },

      {
        signed_byte,
        6,
      },

      {
        byte,
        8,
      },
    },
  };

  const auto a = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(ranges[0], 2), 9);
  const auto b = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(fields[0], 5), 9);
  const auto& representation = prepare(a);

  EXPECT(representation.compatible(prepare(b)));

  Count seen = 0;
  const auto status =
      representation.visit([&](Form::Representation::Position position) {
        EXPECT_EQ(position.offset, seen * 2);
        ++seen;
        return Status::Success;
      });

  EXPECT(status == Status::Success);
  EXPECT_EQ(seen, Count(5));
}

// Source ordering and allocation identity cannot influence the body indices.
// Two independent cell declarations share one body in either source order.
VALIDATION_TEST(TtxEncoding, independent_bodies) {
  Preparation prepare;
  const Form::Schema::Position member(integer, 0);
  const auto cell = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(member, 1), 4, 4);
  auto other_cell = cell;
  const Storage::Vector<Form::Schema::Position, 2> ordered = {
    {
      Form::Schema::Position(cell, 0),
      {
        other_cell,
        8,
      },
    },
  };

  const Storage::Vector<Form::Schema::Position, 2> reversed = {
    {
      Form::Schema::Position(other_cell, 8),
      {
        cell,
        0,
      },
    },
  };

  const auto a = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(ordered[0], 2), 12, 4);
  const auto b = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(reversed[0], 2), 12, 4);

  EXPECT(prepare(a).compatible(prepare(b)));
  EXPECT_EQ(prepare(a).get_bytes().get_size(), Count(16));
}

VALIDATION_TEST(TtxEncoding, payload_endian) {
  Preparation prepare;
  const auto big_byte = Form::Schema::primitive(
      Form::Schema::Value::U8, Form::Schema::ByteOrder::Big);
  const auto big_integer = Form::Schema::primitive(
      Form::Schema::Value::U32, Form::Schema::ByteOrder::Big);

  EXPECT(prepare(byte).compatible(prepare(big_byte)));
  EXPECT_NOT(prepare(integer).compatible(prepare(big_integer)));
  prepare(big_integer)
      .next(0)
      .visit(
          [&](Form::Representation::Position position) {
            EXPECT(position.get_byte_order() == Form::Schema::ByteOrder::Big);
          },

          [&](Status) { EXPECT(false); });
}

// The compiler's coordinates are Count values, even when a descriptor uses a
// wider block. The largest native count exercises a 256 bit block without
// allocating payload storage or overflowing the final occupied byte.
VALIDATION_TEST(TtxEncoding, native_count_limit) {
  Preparation prepare;
  const auto huge = Form::Schema::range(byte, Count(-1), 1, Count(-1));
  const auto& representation = prepare(huge);

  EXPECT_EQ(representation.get_depth(), U8(8));
  EXPECT_EQ(representation.get_bytes().get_size(), Count(64));
  representation.next(Count(-2)).visit(
      [&](Form::Representation::Position position) {
        EXPECT_EQ(position.offset, Count(-2));
      },

      [&](Status) { EXPECT(false); });

  representation.next(Count(-1)).visit(
      [&](Form::Representation::Position) { EXPECT(false); },
      [&](Status status) { EXPECT(status == Status::Bounds); });
}

// Final storage is requested after successful Schema validation. Rejection
// keeps both the output slot and the allocator's allocation count unchanged.
VALIDATION_TEST(TtxEncoding, final_allocation) {
  struct Allocation {
    alignas(Form::Representation) Storage::Vector<U8, 128> bytes;
    Count allocations = 0;

    static auto allocate(void* context, Count size, Count) -> void* {
      auto& allocation = *Data::cast<Allocation>(context);
      ++allocation.allocations;
      return size <= sizeof(allocation.bytes) ? allocation.bytes.get_data()
                                              : nullptr;
    }
  } allocation;

  const ttx_representation_allocator allocator =
      ttx_representation_allocator(&allocation, Allocation::allocate);
  const Form::Representation* output = nullptr;
  auto invalid = integer;
  invalid.data.value.type = 11;

  EXPECT(
      ttx_representation_compile(&invalid, sizeof(void*), allocator, &output) ==
      TTX_DATA_INVALID);
  EXPECT_EQ(allocation.allocations, Count(0));
  EXPECT(output == nullptr);

  ASSERT(
      ttx_representation_compile(&integer, sizeof(void*), allocator, &output) ==
      TTX_DATA_SUCCESS);
  EXPECT_EQ(allocation.allocations, Count(1));
  EXPECT(output->compatible(Form::Compiled<integer>::get_representation()));
}
