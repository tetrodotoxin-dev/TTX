// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/storage/vector.hpp"

#include "ttx/data/form/representation.hpp"
#include "validation/tests/data/form/preparation.hpp"
#include "validation/tests/data/form/visitation.h"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Data;
using namespace Validation::DataTests;

static Harness TtxNavigation = {
  .name = "TTX::Data::Form::Navigation",
};

static constexpr auto integer =
    Form::Schema::primitive(Form::Schema::Value::U32);
static constexpr auto small = Form::Schema::primitive(Form::Schema::Value::U16);

// A byte coordinate selects the last element of a large range directly from
// its stride. Neither this lookup nor the descriptor expands the other values.
VALIDATION_TEST(TtxNavigation, range_coordinate) {
  Preparation prepare;
  const auto range = Form::Schema::range(integer, 1000000000, 4, 4000000000, 4);
  const auto& prepared = prepare(range);
  EXPECT_EQ(prepared.get_bytes().get_size(), Count(32));

  prepared.next(3999999996)
      .visit(
          [&](const Form::Representation::Position& answer) {
            EXPECT(answer.get_value() == integer.get_value());
            EXPECT_EQ(answer.get_offset(), Count(3999999996));
          },

          [&](Status) { EXPECT(false); });

  prepared.next(4000000000)
      .visit(
          [&](auto) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Bounds); });
}

// A nested field is addressed in the enclosing record. Descent adds
// the child placement to its own member offset, preserving leading padding.
VALIDATION_TEST(TtxNavigation, composite_coordinate) {
  Preparation prepare;
  const Storage::Vector<Form::Schema::Position, 2> members = {
    {
      Form::Schema::Position(small, 0),
      {
        integer,
        8,
      },
    },
  };

  const auto record =
      Form::Schema::composite(members.get_view().slice(0, 2), 16, 4);
  const Storage::Vector<Form::Schema::Position, 1> entries = {
    {
      Form::Schema::Position(record, 16),
    },
  };

  const auto root =
      Form::Schema::composite(entries.get_view().slice(0, 1), 32, 4);
  const auto& prepared = prepare(root);

  prepared.next(24).visit(
      [&](const Form::Representation::Position& answer) {
        EXPECT(answer.get_value() == integer.get_value());
        EXPECT_EQ(answer.get_offset(), Count(24));
      },

      [&](Status) { EXPECT(false); });
}

// Whole traversal consumes each primitive occurrence in storage order. The
// visitor keeps only a local expected count, while Representation retains the
// active composite path and performs no repeated lookup of earlier fields.
VALIDATION_TEST(TtxNavigation, padded_range_walk) {
  Preparation prepare;
  const Storage::Vector<Form::Schema::Position, 2> fields = {
    {
      Form::Schema::Position(small, 0),
      {
        integer,
        8,
      },
    },
  };

  const auto record =
      Form::Schema::composite(fields.get_view().slice(0, 2), 16, 4);
  const auto records = Form::Schema::range(record, 16, 16, 256, 4);
  const auto& prepared = prepare(records);
  Count seen = 0;

  const auto status =
      prepared.visit([&](Form::Representation::Position position) {
        const auto expected =
            seen % 2 ? integer.get_value() : small.get_value();
        EXPECT(position.get_value() == expected);
        EXPECT_EQ(position.get_offset(), (seen / 2) * 16 + (seen % 2 ? 8 : 0));
        ++seen;
        return Status::Success;
      });

  EXPECT(status == Status::Success);
  EXPECT_EQ(seen, Count(32));
}

// Physical traversal finds an occupied start at or after the requested byte.
// Checking every byte distinguishes a valid value from its interior bytes and
// surrounding padding. Consumers needing an exact start can compare the
// returned coordinate with their requested offset.
VALIDATION_TEST(TtxNavigation, physical_coordinates) {
  Preparation prepare;
  const Storage::Vector<Form::Schema::Position, 2> fields = {
    {
      Form::Schema::Position(small, 0),
      {
        integer,
        8,
      },
    },
  };

  const auto record =
      Form::Schema::composite(fields.get_view().slice(0, 2), 16, 4);
  const auto records = Form::Schema::range(record, 4, 16, 64, 4);
  const auto& prepared = prepare(records);

  for (Count offset = 0; offset <= records.get_extent(); ++offset) {
    const Count local = offset % 16;
    const Count expected = offset - local + (!local ? 0 : local <= 8 ? 8 : 16);
    prepared.next(offset).visit(
        [&](const Form::Representation::Position& answer) {
          EXPECT(expected < 64);
          EXPECT_EQ(answer.get_offset(), expected);
        },

        [&](Status status) {
          EXPECT(expected >= 64);
          EXPECT(status == Status::Bounds);
        });
  }
}

// Empty has no occupied position. Invalid C arguments are rejected at the
// entry, while callers holding a Representation can use the typed operations.
VALIDATION_TEST(TtxNavigation, empty_and_invalid) {
  Preparation prepare;
  const auto empty =
      Form::Schema::composite(View::Vector<Form::Schema::Position>(), 0);
  const auto& prepared = prepare(empty);

  prepared.next(0).visit(
      [&](auto) { EXPECT(false); },
      [&](Status status) { EXPECT(status == Status::Bounds); });

  ttx_representation_position answer;
  EXPECT(ttx_representation_next(nullptr, 0, &answer) == TTX_DATA_INVALID);
  EXPECT(
      ttx_representation_next(&prepared.get_abi(), 0, nullptr) ==
      TTX_DATA_INVALID);
}

// Deep nesting still accumulates each placement along the selected path. No
// preceding subtree has to be counted to return an ABI byte coordinate.
VALIDATION_TEST(TtxNavigation, deep_coordinate) {
  Preparation prepare;
  Storage::Vector<Form::Schema, 256> layers;
  Storage::Vector<Form::Schema::Position, 256> entries;
  for (Count i = 0; i < 256; ++i) {
    entries[i] = {
      {i ? &layers[i - 1].get_abi() : &integer.get_abi(), 0},
      4,
    };

    layers[i] = Form::Schema::composite({entries[i], 1}, (i + 2) * 4, 4);
  }

  const auto& root = layers[255];
  const auto& prepared = prepare(root);
  prepared.next(0).visit(
      [&](const Form::Representation::Position& answer) {
        EXPECT(answer.get_value() == integer.get_value());
        EXPECT_EQ(answer.get_offset(), Count(1024));
      },

      [&](Status) { EXPECT(false); });
}

// Sharing keeps this binary tree small in the descriptor stream. A coordinate
// selects one path without counting either sibling's implied observations.
// The visitor separately proves that a consumer can stop after a short prefix
// instead of enumerating the entire advertised payload before its first call.
VALIDATION_TEST(TtxNavigation, shared_body_access) {
  Preparation prepare;
  const auto byte = Form::Schema::primitive(Form::Schema::Value::U8);
  Storage::Vector<Form::Schema, 24> bodies;
  Storage::Vector<Storage::Vector<Form::Schema::Position, 3>, 24> fields;
  for (Count i = 0; i < 24; ++i) {
    const auto& child = i ? bodies[i - 1] : byte;
    const Count extent = child.get_extent();
    fields[i][0] = Form::Schema::Position(child, 0);
    fields[i][1] = Form::Schema::Position(byte, extent);
    fields[i][2] = Form::Schema::Position(child, extent + 1);
    bodies[i] = Form::Schema::composite(
        View::Vector<Form::Schema::Position>(fields[i][0], 3), extent * 2 + 1);
  }

  const auto& representation = prepare(bodies[23]);
  const Count last = bodies[23].get_extent() - 1;
  ASSERT(representation.get_bytes().get_size() < 2048);
  representation.next(last).visit(
      [&](Form::Representation::Position position) {
        EXPECT_EQ(position.get_offset(), last);
        EXPECT(position.get_value() == Form::Schema::Value::U8);
      },

      [&](Status) { EXPECT(false); });

  Count seen = 0;
  const auto status =
      representation.visit([&](Form::Representation::Position position) {
        EXPECT_EQ(position.get_offset(), seen);
        ++seen;
        return seen == 9 ? Status::Denied : Status::Success;
      });

  EXPECT(status == Status::Denied);
  EXPECT_EQ(seen, Count(9));
}

// The callback is compiled as C and borrows its state only for this call. A
// failure after two observations prevents a third, while a later call starts
// a fresh walk over the same borrowed descriptor.
VALIDATION_TEST(TtxNavigation, c_streaming_visit) {
  Preparation prepare;
  const auto& representation =
      prepare(Form::Schema::range(integer, 4, 4, 16, 4));
  visitation_probe probe = {
    .stop = 2,
  };

  EXPECT(
      observe_representation(&representation.get_abi(), nullptr, 0, &probe) ==
      TTX_DATA_DENIED);
  EXPECT_EQ(probe.count, Count(2));
  EXPECT_EQ(probe.offsets[1], Count(4));

  probe.count = 0;
  probe.stop = 8;
  EXPECT(
      observe_representation(&representation.get_abi(), nullptr, 0, &probe) ==
      TTX_DATA_SUCCESS);
  EXPECT_EQ(probe.count, Count(4));
  EXPECT_EQ(probe.offsets[3], Count(12));
}

// Selecting the last value of a Range must skip its unrequested instances.
// The C callback sees one value, while malformed exact coordinates yield no
// callback. Neither case constructs an inventory proportional to the Range.
VALIDATION_TEST(TtxNavigation, c_selected_visit) {
  Preparation prepare;
  const auto& representation =
      prepare(Form::Schema::range(integer, 1000000000, 4, 4000000000, 4));
  const Storage::Vector<Count, 1> last = {
    {
      3999999996,
    },
  };

  visitation_probe probe = {
    .stop = 8,
  };

  EXPECT(
      observe_representation(
          &representation.get_abi(), last.get_data(), 1, &probe) ==
      TTX_DATA_SUCCESS);
  EXPECT_EQ(probe.count, Count(1));
  EXPECT_EQ(probe.offsets[0], last[0]);

  const Storage::Vector<Count, 1> interior = {
    {
      1,
    },
  };

  probe.count = 0;
  EXPECT(
      observe_representation(
          &representation.get_abi(), interior.get_data(), 1, &probe) ==
      TTX_DATA_BOUNDS);
  EXPECT_EQ(probe.count, Count(0));

  const Storage::Vector<Count, 1> outside = {
    {
      4000000000,
    },
  };

  EXPECT(
      observe_representation(
          &representation.get_abi(), outside.get_data(), 1, &probe) ==
      TTX_DATA_BOUNDS);
  EXPECT_EQ(probe.count, Count(0));
}

// Offsets 4 and 36 select fields in the first and third copies of a struct.
// Both use the same stored description, so navigation must add each copy's
// starting offset. Offset 12 selects padding and must return Bounds.
VALIDATION_TEST(TtxNavigation, selected_composites) {
  Preparation prepare;
  const Form::Schema::Position member(integer, 4);
  const auto record = Form::Schema::composite(
      View::Vector<Form::Schema::Position>(member, 1), 12, 4);
  const auto& representation =
      prepare(Form::Schema::range(record, 3, 16, 44, 4));
  const Storage::Vector<Count, 2> selected = {
    {
      4,
      36,
    },
  };

  visitation_probe probe = {
    .stop = 8,
  };

  EXPECT(
      observe_representation(
          &representation.get_abi(), selected.get_data(), 2, &probe) ==
      TTX_DATA_SUCCESS);
  EXPECT_EQ(probe.offsets[0], Count(4));
  EXPECT_EQ(probe.offsets[1], Count(36));

  const Storage::Vector<Count, 1> padding = {
    {
      12,
    },
  };

  probe.count = 0;
  EXPECT(
      observe_representation(
          &representation.get_abi(), padding.get_data(), 1, &probe) ==
      TTX_DATA_BOUNDS);
  EXPECT_EQ(probe.count, Count(0));
}
