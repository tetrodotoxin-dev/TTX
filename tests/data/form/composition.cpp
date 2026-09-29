// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tests/data/form/composition.h"

#include "perimortem/core/static/vector.hpp"

#include "tests/data/form/preparation.hpp"
#include "toolchain/validation/unit_test.hpp"

using namespace Perimortem;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static Toolchain::Validation::Harness Composition = {
  .name = "TTX::Data::Form::Composition",
};

// Composition gives each imported root a real member boundary. Compare with
// source compilation to catch lost padding, reference remapping and duplicate
// bodies. Reversed placement order must produce the same canonical bytes.
static auto check_pair(
    const Schema& child,
    Toolchain::Validation::Test::TestResult& result) -> void {
  Validation::DataTests::Preparation prepare;
  const auto& form = prepare(child);
  const Count width = child.get_extent();
  const Perimortem::Core::Static::Vector<Schema::Position, 2> positions = {
    {
      Schema::Position(child, 0),
      {
        child,
        width,
      },
    },
  };
  const auto& expected = prepare(
      Schema::composite(
          {
            positions.get_data(),
            2,
          },
          width * 2, child.get_alignment()));
  const Perimortem::Core::Static::Vector<Representation::Member, 2> members = {
    {
      Representation::Member(form, width),
      {
        form,
        0,
      },
    },
  };
  Memory::Allocator::Arena arena;
  Representation::compose(
      {
        members.get_data(),
        2,
      },
      width * 2, child.get_alignment(), arena)
      .visit(
          [&](const Representation& actual) {
            EXPECT(actual.compatible(expected));
          },
          [&](Status) { EXPECT(False); });

  const ttx_representation_allocator allocator = ttx_representation_allocator(
      &arena, [](void* owner, Count size, Count) -> void* {
        return static_cast<Memory::Allocator::Arena*>(owner)
            ->allocate(size)
            .get_data();
      });
  const Representation* output = nullptr;
  ASSERT(
      compose_pair(&form, width, child.get_alignment(), allocator, &output) ==
      TTX_DATA_SUCCESS);
  EXPECT(output->compatible(expected));
}

VALIDATION_TEST(Composition, records_and_cycles) {
  const auto integer = Schema::primitive(Schema::Value::U32);
  const Perimortem::Core::Static::Vector<Schema::Position, 2> fields = {
    {
      Schema::Position(integer, 0),
      {
        integer,
        8,
      },
    },
  };
  check_pair(
      Schema::composite(
          {
            fields.get_data(),
            2,
          },
          16, 8),
      result);

  auto recursive = Schema::composite(
      Perimortem::Core::View::Vector<Schema::Position>(), 8, 8);
  const Schema::Position pointer(Schema::pointer(&recursive), 0);
  recursive = Schema::composite(
      {
        &pointer,
        1,
      },
      8, 8);
  check_pair(recursive, result);

  // Callable returns and arguments reference the same recursive body. Neither
  // navigation nor composition may mistake their descriptions for payload.
  const Perimortem::Core::Static::Vector<Schema::Argument, 2> args = {
    {
      Schema::pointer(&recursive),
      Schema::Argument(integer),
    },
  };
  const auto function = Schema::callable(
      Schema::Convention::SystemVAMD64,
      {
        args.get_data(),
        2,
      },
      Schema::pointer(&recursive));
  const auto no_arguments =
      Schema::callable(Schema::Convention::SystemVAMD64, {});
  const Perimortem::Core::Static::Vector<Schema::Position, 2> calls = {
    {
      Schema::Position(function, 0),
      {
        no_arguments,
        8,
      },
    },
  };
  check_pair(
      Schema::composite(
          {
            calls.get_data(),
            2,
          },
          16, 8),
      result);
}

VALIDATION_TEST(Composition, compact_ranges) {
  Validation::DataTests::Preparation prepare;
  const auto integer = Schema::primitive(Schema::Value::U32);
  const auto range = Schema::range(integer, 1000000000, 4, 4000000000ULL, 4);
  // A compiled Range has a root covering the whole form. Embedding that form is
  // equivalent to an explicit struct around the range, not flattening away its
  // boundary.
  const Schema::Position field(range, 0);
  check_pair(
      Schema::composite(
          {
            &field,
            1,
          },
          range.get_extent(), 4),
      result);
}

VALIDATION_TEST(Composition, invalid_placements) {
  Validation::DataTests::Preparation prepare;
  const auto integer = Schema::primitive(Schema::Value::U32);
  const auto& form = prepare(integer);
  const Perimortem::Core::Static::Vector<Representation::Member, 2> members = {
    {
      Representation::Member(form, 0),
      {
        form,
        2,
      },
    },
  };
  Memory::Allocator::Arena arena;
  const Perimortem::Core::Static::Vector<Count, 2> extents = {
    {
      5,
      8,
    },
  };
  for (const Count extent : extents.get_view()) {
    Representation::compose(
        {
          members.get_data(),
          2,
        },
        extent, 4, arena)
        .visit(
            [&](const Representation&) { EXPECT(False); },
            [&](Status status) { EXPECT(status == Status::Invalid); });
  }

  Representation::compose(
      Perimortem::Core::View::Vector<Representation::Member>(), 8, 8, arena)
      .visit(
          [&](const Representation&) { EXPECT(False); },
          [&](Status status) { EXPECT(status == Status::Invalid); });
  Representation::compose(
      Perimortem::Core::View::Vector<Representation::Member>(), 0, 1, arena)
      .visit(
          [&](const Representation& empty) {
            EXPECT_EQ(empty.get_extent(), Count(0));
          },
          [&](Status) { EXPECT(False); });
}
