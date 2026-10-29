// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/tests/data/form/composition.h"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/storage/vector.hpp"
#include "perimortem/memory/allocator/arena.hpp"

#include "validation/tests/data/form/preparation.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Toolchain::Validation;
using namespace Ttx::Data;

static Harness Composition = {
  .name = "TTX::Data::Form::Composition",
};

static auto allocate_representation(void* context, Count size, Count alignment)
    -> void* {
  return Data::cast<Allocator::Arena>(context)->allocate(size).get_data();
}

// Composition gives each imported root a real member boundary. Compare with
// source compilation to catch lost padding, reference remapping and duplicate
// bodies. Reversed placement order must produce the same canonical bytes.
static auto check_pair(const Form::Schema& child, Test::TestResult& result)
    -> void {
  Validation::DataTests::Preparation prepare(8);
  const auto& form = prepare(child);
  const Count width = child.get_extent();
  const Storage::Vector<Form::Schema::Position, 2> positions = {
    {
      Form::Schema::Position(child, 0),
      {
        child,
        width,
      },
    },
  };

  const auto& expected = prepare(
      Form::Schema::composite(
          positions.get_view().slice(0, 2), width * 2, child.get_alignment()));
  const Storage::Vector<Form::Representation::Member, 2> members = {
    {
      Form::Representation::Member(&form.get_abi(), width),
      {
        &form.get_abi(),
        0,
      },
    },
  };

  Form::Representation::compose(
      members.get_view().slice(0, 2), width * 2, child.get_alignment())
      .visit(
          [&](const Form::Representation& actual) {
            EXPECT(actual.compatible(expected));
          },

          [&](Status) { EXPECT(False); });

  Allocator::Arena arena;
  const ttx_representation_allocator allocator =
      ttx_representation_allocator(&arena, allocate_representation);
  const ttx_representation* output = nullptr;
  ASSERT(
      compose_pair(
          &form.get_abi(), width, child.get_alignment(), allocator, &output) ==
      TTX_DATA_SUCCESS);
  EXPECT(Form::Representation(*output).compatible(expected));
}

// The composed form survives replacement and destruction of its children.
// Moving it out of the Result keeps it available after the temporary member
// records and both compilation Results have been destroyed.
VALIDATION_TEST(Composition, independent_lifetime) {
  Form::Representation composed;
  {
    const auto integer = Form::Schema::primitive(Form::Schema::Value::U32);
    Form::Representation child;
    Form::Representation::compile(integer).visit(
        [&](Form::Representation& value) { child = Data::take(value); },
        [&](Status status) { EXPECT(False); });
    ASSERT(child.get_bytes().get_size());
    const Form::Representation::Member members[] = {
      {&child.get_abi(), 0}, {&child.get_abi(), 8}};

    Form::Representation::compose({members[0], 2}, 12, 4)
        .visit(
            [&](Form::Representation& value) { composed = Data::take(value); },
            [&](Status status) { EXPECT(False); });
    ASSERT(composed.get_bytes().get_size());
    EXPECT(composed.get_bytes().get_data() != child.get_bytes().get_data());

    child = Form::Representation();
  }

  EXPECT_EQ(composed.get_extent(), Count(12));
  EXPECT_EQ(composed.get_alignment(), Count(4));

  Count visited = 0;
  EXPECT(composed.visit([&](Form::Representation::Position position) {
    EXPECT_EQ(position.get_offset(), visited * 8);
    EXPECT(position.get_value() == Form::Schema::Value::U32);
    ++visited;
    return Status::Success;
  }) == Status::Success);

  EXPECT_EQ(visited, Count(2));
}

VALIDATION_TEST(Composition, records_and_cycles) {
  const auto integer = Form::Schema::primitive(Form::Schema::Value::U32);
  const Storage::Vector<Form::Schema::Position, 2> fields = {
    {
      Form::Schema::Position(integer, 0),
      {
        integer,
        8,
      },
    },
  };

  check_pair(
      Form::Schema::composite(fields.get_view().slice(0, 2), 16, 8), result);

  auto recursive =
      Form::Schema::composite(View::Vector<Form::Schema::Position>(), 8, 8);
  const Form::Schema::Position pointer(Form::Schema::pointer(&recursive), 0);
  recursive = Form::Schema::composite({pointer, 1}, 8, 8);
  check_pair(recursive, result);

  // Callable returns and arguments reference the same recursive body. Neither
  // navigation nor composition may mistake their descriptions for payload.
  const Storage::Vector<Form::Schema::Argument, 2> args = {
    {
      Form::Schema::Argument{Form::Schema::pointer(&recursive), 1},
      Form::Schema::Argument(integer, 1),
    },
  };

  const auto function = Form::Schema::callable(
      Form::Callable::Convention::SystemVAMD64, args.get_view().slice(0, 2),
      Form::Schema::pointer(&recursive), 8);
  const auto no_arguments = Form::Schema::callable(
      Form::Callable::Convention::SystemVAMD64, {}, {}, 8);
  const Storage::Vector<Form::Schema::Position, 2> calls = {
    {
      Form::Schema::Position(function, 0),
      {
        no_arguments,
        8,
      },
    },
  };

  check_pair(
      Form::Schema::composite(calls.get_view().slice(0, 2), 16, 8), result);
}

VALIDATION_TEST(Composition, compact_ranges) {
  const auto integer = Form::Schema::primitive(Form::Schema::Value::U32);
  const auto range =
      Form::Schema::range(integer, 1000000000, 4, 4000000000ULL, 4);
  // A compiled Range has a root covering the whole form. Embedding that form is
  // equivalent to an explicit struct around the range, not flattening away its
  // boundary.
  const Form::Schema::Position field(range, 0);
  check_pair(
      Form::Schema::composite({field, 1}, range.get_extent(), 4), result);
}

VALIDATION_TEST(Composition, invalid_placements) {
  Validation::DataTests::Preparation prepare;
  const auto integer = Form::Schema::primitive(Form::Schema::Value::U32);
  const auto& form = prepare(integer);
  const Storage::Vector<Form::Representation::Member, 2> members = {
    {
      Form::Representation::Member(&form.get_abi(), 0),
      {
        &form.get_abi(),
        2,
      },
    },
  };

  const Storage::Vector<Count, 2> extents = {
    {
      5,
      8,
    },
  };

  for (Count extent_index = 0; extent_index < extents.get_size();
       ++extent_index) {
    const Count extent = extents[extent_index];
    Form::Representation::compose(members.get_view().slice(0, 2), extent, 4)
        .visit(
            [&](const Form::Representation& representation) { EXPECT(False); },
            [&](Status status) { EXPECT(status == Status::Invalid); });
  }

  Form::Representation::compose(
      View::Vector<Form::Representation::Member>(), 8, 8)
      .visit(
          [&](const Form::Representation& representation) { EXPECT(False); },
          [&](Status status) { EXPECT(status == Status::Invalid); });
  Form::Representation::compose(
      View::Vector<Form::Representation::Member>(), 0, 1)
      .visit(
          [&](const Form::Representation& empty) {
            EXPECT_EQ(empty.get_extent(), Count(0));
          },

          [&](Status) { EXPECT(False); });
}
