// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include <stddef.h>

#include "perimortem/core/static/vector.hpp"

#include "tests/data/form/preparation.hpp"
#include "toolchain/validation/unit_test.hpp"
#include "ttx/data/form/schema.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;
using Ttx::Data::Form::Schema;
using Validation::DataTests::Preparation;

static Toolchain::Validation::Harness TtxCompiler = {
  .name = "TTX::Data::Form::Compiler",
};
static constexpr auto integer = Schema::primitive(Schema::Value::U32);

// Both encodings describe four adjacent U32s. One stores four entries and the
// other stores one repeated entry. Different root and leaf addresses establish
// that pointer identity is only a fast path, never required for agreement.
VALIDATION_TEST(TtxCompiler, composite_and_range) {
  Preparation prepare;
  auto foreign_integer = integer;
  const Static::Vector<Schema::Position, 4> entries = {
    {
      Schema::Position(integer, 0),
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
  const auto flat = Schema::composite(
      {
        entries.get_data(),
        4,
      },
      16, 4);
  const auto range = Schema::range(foreign_integer, 4, 4, 16, 4);

  EXPECT(prepare(flat).compatible(prepare(range)));
  EXPECT(prepare(range).compatible(prepare(flat)));

  auto changed = foreign_integer;
  changed.data.value.type = static_cast<U8>(Schema::Value::R32);
  const auto incompatible = Schema::range(changed, 4, 4, 16, 4);
  EXPECT_NOT(prepare(flat).compatible(prepare(incompatible)));
}

// Admission catches incorrect fixed facts once. Runtime operations require an
// admitted immutable descriptor so they never repeat this walk for each GEP.
VALIDATION_TEST(TtxCompiler, schema_validation) {
  Preparation prepare;
  Static::Vector<Schema::Position, 2> entries = {
    {
      Schema::Position(integer, 0),
      {
        integer,
        2,
      },
    },
  };
  auto shape = Schema::composite(
      {
        entries.get_data(),
        2,
      },
      8, 4);
  EXPECT(prepare.validate(shape) == Status::Invalid);

  entries[1].offset = 4;
  EXPECT(prepare.validate(shape) == Status::Success);

  entries[0].reference = &shape;
  EXPECT(prepare.validate(shape) == Status::Invalid);

  const auto huge = Schema::range(integer, Count(-1), 4, Count(-1), 4);
  EXPECT(prepare.validate(huge) == Status::Overflow);
}

// Factoring a range into pairs must not turn contract comparison into a walk
// over all billion values. These schemas have the same byte sequence but use
// different repetition factors and independently constructed leaf records.
VALIDATION_TEST(TtxCompiler, factored_ranges) {
  Preparation prepare;
  const auto other = Schema::primitive(Schema::Value::U32);
  const auto pair = Schema::range(integer, 2, 4, 8, 4);
  const auto nested = Schema::range(pair, 500000000, 8, 4000000000, 4);
  const auto flat = Schema::range(other, 1000000000, 4, 4000000000, 4);

  EXPECT(prepare(nested).compatible(prepare(flat)));
}

// Alternating primitive types prevent compaction from hiding the width of the
// record. Agreement and lookup must preserve all 512 descriptors. Scaling is
// measured separately by the compiler benchmarks.
VALIDATION_TEST(TtxCompiler, wide_composite) {
  Preparation prepare;
  const auto other = Schema::primitive(Schema::Value::U32);
  const auto real = Schema::primitive(Schema::Value::R32);
  Static::Vector<Schema::Position, 512> a, b;
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

  const auto left = Schema::composite(
      {
        a.get_data(),
        512,
      },
      2048, 4);
  const auto right = Schema::composite(
      {
        b.get_data(),
        512,
      },
      2048, 4);

  const auto& prepared_left = prepare(left);
  const auto& prepared_right = prepare(right);
  EXPECT(prepared_left.compatible(prepared_right));
  EXPECT_EQ(prepared_left.get_bytes().get_size(), Count(513 * 8));
  prepared_right.next(2044).visit(
      [&](const Representation::Position& position) {
        EXPECT_EQ(position.offset, Count(2044));
      },
      [&](Status) { EXPECT(false); });
}

// Every source spelling of no values compiles to the same Empty form. Neither
// an unused Range element nor another layer of grouping creates a position.
// The full matrix catches agreement that works only through a third shape.
VALIDATION_TEST(TtxCompiler, empty_equivalence) {
  Preparation prepare;
  const auto real = Schema::primitive(Schema::Value::R32);
  const Static::Vector<Schema, 3> forms = {
    {
      Schema::range(integer, 0, 4, 0, 4),
      Schema::composite(
          Perimortem::Core::View::Vector<Schema::Position>(), 0, 4),
      Schema::range(real, 0, 4, 0, 4),
    },
  };
  Static::Vector<const Representation*, 3> prepared = {
    {
      &prepare(forms[0]),
      &prepare(forms[1]),
      &prepare(forms[2]),
    },
  };
  for (const auto* source : prepared.get_view()) {
    for (const auto* destination : prepared.get_view()) {
      EXPECT(source->compatible(*destination));
    }
  }
}

// A child body includes its own extent. Moving padding out of that body
// changes its descriptor, even when the parent has the same occupied bytes.
// Explicitly placed compact children still agree with their repeated form.
VALIDATION_TEST(TtxCompiler, padding_equivalence) {
  Preparation prepare;
  const Static::Vector<Schema::Position, 1> child = {
    {
      Schema::Position(integer, 0),
    },
  };
  const auto padded = Schema::composite(
      {
        child.get_data(),
        1,
      },
      8, 4);
  const auto compact = Schema::composite(
      {
        child.get_data(),
        1,
      },
      4, 4);
  const Static::Vector<Schema::Position, 2> nested = {
    {
      Schema::Position(padded, 0),
      {
        compact,
        8,
      },
    },
  };
  const Static::Vector<Schema::Position, 2> flat = {
    {
      Schema::Position(compact, 0),
      {
        compact,
        8,
      },
    },
  };
  const Static::Vector<Schema, 3> forms = {
    {
      Schema::composite(
          {
            nested.get_data(),
            2,
          },
          12, 4),
      Schema::composite(
          {
            flat.get_data(),
            2,
          },
          12, 4),
      Schema::range(compact, 2, 8, 12, 4),
    },
  };
  EXPECT_NOT(prepare(forms[0]).compatible(prepare(forms[1])));
  EXPECT(prepare(forms[1]).compatible(prepare(forms[2])));

  const Static::Vector<Schema::Position, 2> shifted = {
    {
      Schema::Position(compact, 4),
      {
        compact,
        8,
      },
    },
  };
  const auto different = Schema::composite(
      {
        shifted.get_data(),
        2,
      },
      12, 4);
  EXPECT_NOT(prepare(forms[0]).compatible(prepare(different)));
}

// Repetition retains a child body's extent even when its primitive values
// occupy the same coordinates. A billion instances still contribute just
// their repeated descriptor and child body to the comparison.
VALIDATION_TEST(TtxCompiler, repeated_padding) {
  Preparation prepare;
  const Static::Vector<Schema::Position, 1> child = {
    {
      Schema::Position(integer, 0),
    },
  };
  const auto padded = Schema::composite(
      {
        child.get_data(),
        1,
      },
      8, 4);
  const auto compact = Schema::composite(
      {
        child.get_data(),
        1,
      },
      4, 4);
  const auto a = Schema::range(padded, 1000000000, 8, 8000000000, 4);
  const auto b = Schema::range(compact, 1000000000, 8, 8000000000, 4);
  EXPECT_NOT(prepare(a).compatible(prepare(b)));
}

// A surrounding Composite is an additional body. Neither its member count
// nor a matching extent allows comparison to discard that boundary.
VALIDATION_TEST(TtxCompiler, nested_group_match) {
  Preparation prepare;
  const Static::Vector<Schema::Position, 2> pair = {
    {
      Schema::Position(integer, 0),
      {
        integer,
        4,
      },
    },
  };
  const auto group = Schema::composite(
      {
        pair.get_data(),
        2,
      },
      8, 4);
  const Static::Vector<Schema::Position, 2> fields = {
    {
      Schema::Position(group, 0),
      {
        integer,
        8,
      },
    },
  };
  const auto flat = Schema::composite(
      {
        fields.get_data(),
        2,
      },
      12, 4);
  const Static::Vector<Schema::Position, 1> wrapper = {
    {
      Schema::Position(flat, 0),
    },
  };
  const auto nested = Schema::composite(
      {
        wrapper.get_data(),
        1,
      },
      12, 4);
  EXPECT_NOT(prepare(nested).compatible(prepare(flat)));

  const auto ungrouped = Schema::range(integer, 3, 4, 12, 4);
  EXPECT_NOT(prepare(nested).compatible(prepare(ungrouped)));
}
