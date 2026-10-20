// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include <stddef.h>

#include "toolchain/validation/test.hpp"

#include "perimortem/core/storage/vector.hpp"

#include "ttx/data/form/schema.hpp"
#include "validation/tests/data/form/preparation.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Data;
using namespace Validation::DataTests;

static Harness TtxCompiler = {
  .name = "TTX::Data::Form::Compiler",
};

static constexpr auto integer =
    Form::Schema::primitive(Form::Schema::Value::U32);

// Both encodings describe four adjacent U32s. One stores four entries and the
// other stores one repeated entry. Different root and leaf addresses establish
// that pointer identity is only a fast path, never required for agreement.
VALIDATION_TEST(TtxCompiler, composite_and_range) {
  Preparation prepare;
  auto foreign_integer = integer;
  const Storage::Vector<Form::Schema::Position, 4> entries = {
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
        12,
      },
    },
  };

  const auto flat =
      Form::Schema::composite(entries.get_view().slice(0, 4), 16, 4);
  const auto range = Form::Schema::range(foreign_integer, 4, 4, 16, 4);

  EXPECT(prepare(flat).compatible(prepare(range)));
  EXPECT(prepare(range).compatible(prepare(flat)));

  auto changed = foreign_integer;
  changed.data.value.type = U8(Form::Schema::Value::R32);

  const auto incompatible = Form::Schema::range(changed, 4, 4, 16, 4);
  EXPECT_NOT(prepare(flat).compatible(prepare(incompatible)));
}

// Compilation checks placement, recursion and range overflow before emitting
// a canonical description. Consumers use that description for subsequent byte
// coordinate lookup without compiling the Schema again.
VALIDATION_TEST(TtxCompiler, schema_validation) {
  Preparation prepare;
  Storage::Vector<Form::Schema::Position, 2> entries = {
    {
      Form::Schema::Position(integer, 0),
      {
        integer,
        2,
      },
    },
  };

  auto shape = Form::Schema::composite(entries.get_view().slice(0, 2), 8, 4);
  EXPECT(prepare.validate(shape) == Status::Invalid);

  entries[1].offset = 4;
  EXPECT(prepare.validate(shape) == Status::Success);

  entries[0].reference = &shape;
  EXPECT(prepare.validate(shape) == Status::Invalid);

  const auto huge = Form::Schema::range(integer, Count(-1), 4, Count(-1), 4);
  EXPECT(prepare.validate(huge) == Status::Overflow);
}

// Factoring a range into pairs must not turn contract comparison into a walk
// over all billion values. These schemas have the same byte sequence but use
// different repetition factors and independently constructed leaf records.
VALIDATION_TEST(TtxCompiler, factored_ranges) {
  Preparation prepare;
  const auto other = Form::Schema::primitive(Form::Schema::Value::U32);
  const auto pair = Form::Schema::range(integer, 2, 4, 8, 4);
  const auto nested = Form::Schema::range(pair, 500000000, 8, 4000000000, 4);
  const auto flat = Form::Schema::range(other, 1000000000, 4, 4000000000, 4);

  EXPECT(prepare(nested).compatible(prepare(flat)));
}

// Alternating primitive types prevent compaction from hiding the width of the
// record. Agreement and lookup must preserve all 512 descriptors. Scaling is
// measured separately by the compiler benchmarks.
VALIDATION_TEST(TtxCompiler, wide_composite) {
  Preparation prepare;
  const auto other = Form::Schema::primitive(Form::Schema::Value::U32);
  const auto real = Form::Schema::primitive(Form::Schema::Value::R32);
  Storage::Vector<Form::Schema::Position, 512> a, b;
  for (Count i = 0; i < 512; ++i) {
    a[i] = {
      i % 2 ? integer : real,
      i * 4,
    };

    b[i] = {
      i % 2 ? other : real,
      i * 4,
    };
  }

  const auto left =
      Form::Schema::composite(a.get_view().slice(0, 512), 2048, 4);
  const auto right =
      Form::Schema::composite(b.get_view().slice(0, 512), 2048, 4);

  const auto& prepared_left = prepare(left);
  const auto& prepared_right = prepare(right);
  EXPECT(prepared_left.compatible(prepared_right));
  EXPECT_EQ(prepared_left.get_bytes().get_size(), Count(513 * 8));
  prepared_right.next(2044).visit(
      [&](const Form::Representation::Position& position) {
        EXPECT_EQ(position.offset, Count(2044));
      },

      [&](Status) { EXPECT(false); });
}

// Every source spelling of no values compiles to the same Empty form. Neither
// an unused Range element nor another layer of grouping creates a position.
// The full matrix catches agreement that works only through a third shape.
VALIDATION_TEST(TtxCompiler, empty_equivalence) {
  Preparation prepare;
  const auto real = Form::Schema::primitive(Form::Schema::Value::R32);
  const Storage::Vector<Form::Schema, 3> forms = {
    {
      Form::Schema::range(integer, 0, 4, 0, 4),
      Form::Schema::composite(View::Vector<Form::Schema::Position>(), 0, 4),
      Form::Schema::range(real, 0, 4, 0, 4),
    },
  };

  Storage::Vector<const Form::Representation*, 3> prepared = {
    {
      &prepare(forms[0]),
      &prepare(forms[1]),
      &prepare(forms[2]),
    },
  };

  for (Count source_index = 0; source_index < prepared.get_size();
       ++source_index) {
    const auto* source = prepared[source_index];
    for (Count destination_index = 0; destination_index < prepared.get_size();
         ++destination_index) {
      const auto* destination = prepared[destination_index];
      EXPECT(source->compatible(*destination));
    }
  }
}

// A child body includes its own extent. Moving padding out of that body
// changes its descriptor, even when the parent has the same occupied bytes.
// Explicitly placed compact children still agree with their repeated form.
VALIDATION_TEST(TtxCompiler, padding_equivalence) {
  Preparation prepare;
  const Storage::Vector<Form::Schema::Position, 1> child = {
    {
      Form::Schema::Position(integer, 0),
    },
  };

  const auto padded =
      Form::Schema::composite(child.get_view().slice(0, 1), 8, 4);
  const auto compact =
      Form::Schema::composite(child.get_view().slice(0, 1), 4, 4);
  const Storage::Vector<Form::Schema::Position, 2> nested = {
    {
      Form::Schema::Position(padded, 0),
      {
        compact,
        8,
      },
    },
  };

  const Storage::Vector<Form::Schema::Position, 2> flat = {
    {
      Form::Schema::Position(compact, 0),
      {
        compact,
        8,
      },
    },
  };

  const Storage::Vector<Form::Schema, 3> forms = {
    {
      Form::Schema::composite(nested.get_view().slice(0, 2), 12, 4),
      Form::Schema::composite(flat.get_view().slice(0, 2), 12, 4),
      Form::Schema::range(compact, 2, 8, 12, 4),
    },
  };

  EXPECT_NOT(prepare(forms[0]).compatible(prepare(forms[1])));
  EXPECT(prepare(forms[1]).compatible(prepare(forms[2])));

  const Storage::Vector<Form::Schema::Position, 2> shifted = {
    {
      Form::Schema::Position(compact, 4),
      {
        compact,
        8,
      },
    },
  };

  const auto different =
      Form::Schema::composite(shifted.get_view().slice(0, 2), 12, 4);
  EXPECT_NOT(prepare(forms[0]).compatible(prepare(different)));
}

// Repetition retains a child body's extent even when its primitive values
// occupy the same coordinates. A billion instances still contribute just
// their repeated descriptor and child body to the comparison.
VALIDATION_TEST(TtxCompiler, repeated_padding) {
  Preparation prepare;
  const Storage::Vector<Form::Schema::Position, 1> child = {
    {
      Form::Schema::Position(integer, 0),
    },
  };

  const auto padded =
      Form::Schema::composite(child.get_view().slice(0, 1), 8, 4);
  const auto compact =
      Form::Schema::composite(child.get_view().slice(0, 1), 4, 4);
  const auto a = Form::Schema::range(padded, 1000000000, 8, 8000000000, 4);
  const auto b = Form::Schema::range(compact, 1000000000, 8, 8000000000, 4);
  EXPECT_NOT(prepare(a).compatible(prepare(b)));
}

// Wrapping a Composite in another struct changes its canonical form even when
// the wrapper adds no bytes. Comparison preserves that extra struct boundary.
VALIDATION_TEST(TtxCompiler, nested_group_match) {
  Preparation prepare;
  const Storage::Vector<Form::Schema::Position, 2> pair = {
    {
      Form::Schema::Position(integer, 0),
      {
        integer,
        4,
      },
    },
  };

  const auto group = Form::Schema::composite(pair.get_view().slice(0, 2), 8, 4);
  const Storage::Vector<Form::Schema::Position, 2> fields = {
    {
      Form::Schema::Position(group, 0),
      {
        integer,
        8,
      },
    },
  };

  const auto flat =
      Form::Schema::composite(fields.get_view().slice(0, 2), 12, 4);
  const Storage::Vector<Form::Schema::Position, 1> wrapper = {
    {
      Form::Schema::Position(flat, 0),
    },
  };

  const auto nested =
      Form::Schema::composite(wrapper.get_view().slice(0, 1), 12, 4);
  EXPECT_NOT(prepare(nested).compatible(prepare(flat)));

  const auto ungrouped = Form::Schema::range(integer, 3, 4, 12, 4);
  EXPECT_NOT(prepare(nested).compatible(prepare(ungrouped)));
}
