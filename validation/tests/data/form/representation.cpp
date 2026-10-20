// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/representation.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/storage/vector.hpp"

#include "ttx/data/form/schema.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Toolchain::Validation;
using namespace Ttx::Data;

static Harness TtxRepresentation = {
  .name = "TTX::Data::Form::Representation",
};

static constexpr auto u32 = Form::Schema::primitive(Form::Schema::Value::U32);

// Both the source tree and its placement array disappear before lookup. The
// prepared result must carry its own facts, including indices derived from the
// source structure. Mutating the original leaf cannot change that canonical
// form.
static auto allocate_representation(void* context, Count size, Count) -> void* {
  return Data::cast<Allocator::Arena>(context)->allocate(size).get_data();
}

VALIDATION_TEST(TtxRepresentation, independent_lifetime) {
  Allocator::Arena arena;
  const Form::Representation* prepared = nullptr;
  {
    auto value = u32;
    const Storage::Vector<Form::Schema::Position, 2> entries = {
      {
        Form::Schema::Position(value, 0),
        {
          value,
          8,
        },
      },
    };

    const auto schema =
        Form::Schema::composite(entries.get_view().slice(0, 2), 12, 4);
    Form::Representation::compile(schema, arena)
        .visit(
            [&](const Form::Representation& result) { prepared = &result; },
            [&](Status) { EXPECT(false); });
    value.data.value.type = TTX_SCHEMA_R32;
  }

  ASSERT(prepared);
  prepared->next(8).visit(
      [&](Form::Representation::Position entry) {
        EXPECT_EQ(entry.offset, Count(8));
        EXPECT(entry.get_value() == Form::Schema::Value::U32);
      },

      [&](Status) { EXPECT(false); });
}

// Alternating types retain separate descriptors. Logical lookup crosses the
// enclosing Composite and returns the selected primitive by value.
VALIDATION_TEST(TtxRepresentation, heterogeneous_index) {
  Allocator::Arena arena;
  const auto real = Form::Schema::primitive(Form::Schema::Value::R32);
  const Storage::Vector<Form::Schema::Position, 2> entries = {
    {
      Form::Schema::Position(u32, 0),
      {
        real,
        8,
      },
    },
  };

  const auto inner =
      Form::Schema::composite(entries.get_view().slice(0, 2), 12, 4);
  const Storage::Vector<Form::Schema::Position, 1> wrapper = {
    {
      Form::Schema::Position(inner, 0),
    },
  };

  const auto outer =
      Form::Schema::composite(wrapper.get_view().slice(0, 1), 12, 4);
  Form::Representation::compile(outer, arena)
      .visit(
          [&](const Form::Representation& value) {
            value.next(8).visit(
                [&](Form::Representation::Position entry) {
                  EXPECT_EQ(entry.offset, Count(8));
                  EXPECT(entry.get_value() == Form::Schema::Value::R32);
                },

                [&](Status) { EXPECT(false); });
          },

          [&](Status) { EXPECT(false); });
}

// Failure leaves the canonical form slot untouched. A caller can discard its
// preparation arena without having lent an incomplete result to consumers.
VALIDATION_TEST(TtxRepresentation, invalid_publication) {
  Allocator::Arena arena;
  auto truncated = u32;
  truncated.extent = 1;

  auto misaligned = u32;
  misaligned.alignment = 1;

  const auto padding =
      Form::Schema::composite(View::Vector<Form::Schema::Position>(), 8, 8);
  const Storage::Vector<Form::Schema, 3> failures = {
    {
      truncated,
      misaligned,
      padding,
    },
  };

  for (Count source_index = 0; source_index < failures.get_size();
       ++source_index) {
    const auto& source = failures[source_index];
    Form::Representation::compile(source, arena)
        .visit(
            [&](const Form::Representation&) { EXPECT(false); },
            [&](Status status) { EXPECT(status == Status::Invalid); });
  }

  Form::Schema cyclic = Form::Schema();
  const Storage::Vector<Form::Schema::Position, 1> entry = {
    {
      Form::Schema::Position(cyclic, 0),
    },
  };

  cyclic = Form::Schema::composite(entry.get_view().slice(0, 1), 4, 4);
  Form::Representation::compile(cyclic, arena)
      .visit(
          [&](const Form::Representation&) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Invalid); });
}

// One repetition has no observable stride. Preparation normalizes that fact
// so both logical and physical lookup reach the value through the same shape.
VALIDATION_TEST(TtxRepresentation, singleton_range) {
  Allocator::Arena arena;
  const auto source = Form::Schema::range(u32, 1, 0, 4, 4);
  Form::Representation::compile(source, arena)
      .visit(
          [&](const Form::Representation& value) {
            value.next(0).visit(
                [&](Form::Representation::Position entry) {
                  EXPECT_EQ(entry.offset, Count(0));
                  EXPECT(entry.get_value() == Form::Schema::Value::U32);
                },

                [&](Status) { EXPECT(false); });
            value.next(1).visit(
                [&](Form::Representation::Position) { EXPECT(false); },
                [&](Status status) { EXPECT(status == Status::Bounds); });
          },

          [&](Status) { EXPECT(false); });
}

// The C entry publishes only after the whole compilation succeeds. Its caller
// may already have a usable canonical form in the slot, which failure must
// retain.
VALIDATION_TEST(TtxRepresentation, publication_on_error) {
  Allocator::Arena arena;
  const Form::Representation sentinel = Form::Representation();
  const Form::Representation* output = &sentinel;
  auto source = u32;
  source.extent = 1;

  const ttx_representation_allocator allocator =
      ttx_representation_allocator(&arena, allocate_representation);
  EXPECT(
      ttx_representation_compile(&source, sizeof(void*), allocator, &output) ==
      TTX_DATA_INVALID);
  EXPECT(output == &sentinel);
}

// Empty members and repetitions normalize to the unit form: extent zero,
// alignment one and eight descriptor bytes. Even a billion repetitions with
// source alignment eight produce that form because they contain no values.
VALIDATION_TEST(TtxRepresentation, nested_units) {
  Allocator::Arena arena;
  const auto empty =
      Form::Schema::composite(View::Vector<Form::Schema::Position>(), 0);
  const auto repeated = Form::Schema::range(empty, 1000000000, 8, 0, 8);
  const Storage::Vector<Form::Schema::Position, 2> children = {
    {
      Form::Schema::Position(empty, 0),
      {
        repeated,
        0,
      },
    },
  };

  const auto grouped =
      Form::Schema::composite(children.get_view().slice(0, 2), 0, 8);
  Form::Representation::compile(grouped, arena)
      .visit(
          [&](const Form::Representation& value) {
            EXPECT_EQ(value.get_bytes().get_size(), Count(8));
            EXPECT_EQ(value.get_extent(), Count(0));
            EXPECT_EQ(value.get_alignment(), Count(1));
          },

          [&](Status) { EXPECT(false); });
}

// Compilation merges neighboring progressions rather than requiring the
// author to spell one large Range. The first value from the second source
// remains observable at byte offset 12 after their descriptors are merged.
VALIDATION_TEST(TtxRepresentation, adjacent_ranges) {
  Allocator::Arena arena;
  const auto first = Form::Schema::range(u32, 3, 4, 12, 4);
  const auto second = Form::Schema::range(u32, 5, 4, 20, 4);
  const Storage::Vector<Form::Schema::Position, 2> pieces = {
    {
      Form::Schema::Position(first, 0),
      Form::Schema::Position(second, 12),
    },
  };

  const auto source = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(pieces[0], 2), 32, 4);

  Form::Representation::compile(source, arena)
      .visit(
          [&](const Form::Representation& prepared) {
            EXPECT_EQ(prepared.get_bytes().get_size(), Count(8));
            prepared.next(12).visit(
                [&](Form::Representation::Position position) {
                  EXPECT_EQ(position.offset, Count(12));
                },

                [&](Status) { EXPECT(false); });
          },

          [&](Status) { EXPECT(false); });
}

// Two different primitive types form one repeated pattern. Neither transfer
// metadata nor boundary metadata may grow with a billion repetitions. The
// last value's physical coordinate distinguishes the pattern stride from
// a packed array of either primitive alone.
VALIDATION_TEST(TtxRepresentation, repeated_records) {
  Allocator::Arena arena;
  Allocator::Arena other_arena;
  const auto real = Form::Schema::primitive(Form::Schema::Value::R32);
  const Storage::Vector<Form::Schema::Position, 2> fields = {
    {
      Form::Schema::Position(u32, 0),
      Form::Schema::Position(real, 8),
    },
  };

  const auto record = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(fields[0], 2), 16, 4);
  const auto source =
      Form::Schema::range(record, 1000000000, 16, 16000000000ULL, 4);

  Form::Representation::compile(source, arena)
      .visit(
          [&](const Form::Representation& prepared) {
            EXPECT_EQ(prepared.get_bytes().get_size(), Count(80));
            Form::Representation::compile(source, other_arena)
                .visit(
                    [&](const Form::Representation& other) {
                      EXPECT(prepared.compatible(other));
                    },

                    [&](Status) { EXPECT(false); });
            prepared.next(15999999992ULL)
                .visit(
                    [&](Form::Representation::Position position) {
                      EXPECT_EQ(position.offset, Count(15999999992ULL));
                    },

                    [&](Status) { EXPECT(false); });
          },

          [&](Status) { EXPECT(false); });
}

// Both records expose the same four U32s, but their struct bodies differ:
// two pairs versus one and three. Those boundaries remain in the descriptor
// stream, so byte comparison rejects the different contracts directly.
VALIDATION_TEST(TtxRepresentation, boundary_mismatch) {
  Allocator::Arena arena;
  const Storage::Vector<Form::Schema::Position, 2> pair_fields = {
    {
      Form::Schema::Position(u32, 0),
      {
        u32,
        4,
      },
    },
  };

  const Storage::Vector<Form::Schema::Position, 3> triple_fields = {
    {
      Form::Schema::Position(u32, 0),
      {
        u32,
        4,
      },

      {
        u32,
        8,
      },
    },
  };

  const Form::Schema::Position single_field(u32, 0);
  const auto pair = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(pair_fields[0], 2), 8, 4);
  const auto triple = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(triple_fields[0], 3), 12, 4);
  const auto single = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(single_field, 1), 4, 4);
  const Storage::Vector<Form::Schema::Position, 2> two_pairs = {
    {
      Form::Schema::Position(pair, 0),
      {
        pair,
        8,
      },
    },
  };

  const Storage::Vector<Form::Schema::Position, 2> one_three = {
    {
      Form::Schema::Position(single, 0),
      {
        triple,
        4,
      },
    },
  };

  const auto a = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(two_pairs[0], 2), 16, 4);
  const auto b = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(one_three[0], 2), 16, 4);

  Form::Representation::compile(a, arena).visit(
      [&](const Form::Representation& left) {
        Form::Representation::compile(b, arena).visit(
            [&](const Form::Representation& right) {
              EXPECT_NOT(left.compatible(right));
              EXPECT_NOT(right.compatible(left));
            },

            [&](Status) { EXPECT(false); });
      },

      [&](Status) { EXPECT(false); });
}

// One author repeats a record containing two nested pairs. Another lists four
// copies explicitly. Compaction and body sharing produce identical streams
// while preserving each nested struct boundary.
VALIDATION_TEST(TtxRepresentation, boundary_patterns) {
  Allocator::Arena arena;
  const Storage::Vector<Form::Schema::Position, 2> pair_fields = {
    {
      Form::Schema::Position(u32, 0),
      {
        u32,
        4,
      },
    },
  };

  const auto pair = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(pair_fields[0], 2), 8, 4);
  const Storage::Vector<Form::Schema::Position, 2> record_fields = {
    {
      Form::Schema::Position(pair, 0),
      {
        pair,
        8,
      },
    },
  };

  const auto record = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(record_fields[0], 2), 16, 4);
  const auto repeated = Form::Schema::range(record, 4, 16, 64, 4);
  const Storage::Vector<Form::Schema::Position, 4> listed = {
    {
      Form::Schema::Position(record, 0),
      {
        record,
        16,
      },

      {
        record,
        32,
      },

      {
        record,
        48,
      },
    },
  };

  const auto explicit_record = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(listed[0], 4), 64, 4);

  Form::Representation::compile(repeated, arena)
      .visit(
          [&](const Form::Representation& left) {
            Form::Representation::compile(explicit_record, arena)
                .visit(
                    [&](const Form::Representation& right) {
                      EXPECT(left.compatible(right));
                      EXPECT(right.compatible(left));
                    },

                    [&](Status) { EXPECT(false); });
          },

          [&](Status) { EXPECT(false); });
}
