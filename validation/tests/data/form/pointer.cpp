// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/storage/vector.hpp"

#include "ttx/data/form/compiled.hpp"
#include "ttx/data/form/compiler.hpp"
#include "validation/tests/data/form/preparation.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Toolchain::Validation;
using namespace Ttx::Data;

static Harness Pointers = {
  .name = "TTX::Data::Form::Pointer",
};

// Constant evaluation may build a temporary reference to itself. Only the
// finished bytes escape preparation, so no constexpr allocation or source
// pointer is retained in the canonical form.
static consteval auto recursive_bytes() -> Storage::Bytes<8> {
  auto node =
      Form::Schema::composite(View::Vector<Form::Schema::Position>(), 8, 8);
  const auto pointer = Form::Schema::pointer(&node);
  const Form::Schema::Position field(pointer, 0);
  node = Form::Schema::composite({field, 1}, 8, 8);

  Form::Compiler<Compile::Vector> compiler;
  if (compiler.compile(node, 8) != Status::Success ||
      compiler.get_size() != 8) {
    return Storage::Bytes<8>();
  }

  Storage::Bytes<8> bytes;
  compiler.write(Access::Bytes(bytes[0], 8));
  return bytes;
}

static constexpr auto recursive = recursive_bytes();
static_assert(recursive[4] == 0x80 && recursive[5] == 0x22);

// One target is used both inline and indirectly. The pointer modifier belongs
// to the reference, so neither use changes or copies that target definition.
VALIDATION_TEST(Pointers, reference_modifier) {
  Validation::DataTests::Preparation prepare(8);
  const auto integer = Form::Schema::primitive(Form::Schema::Value::U32);
  const Form::Schema::Position member(integer, 0);
  const auto target = Form::Schema::composite({member, 1}, 4, 4);
  const Form::Schema::Reference direct(target);
  const auto indirect = Form::Schema::pointer(&target);
  EXPECT(direct.get_abi().schema == indirect.get_abi().schema);
  EXPECT_NOT(direct.is_pointer());
  EXPECT(indirect.is_pointer());

  const Storage::Vector<Form::Schema::Position, 2> fields = {
    {
      Form::Schema::Position(direct, 0),
      {
        indirect,
        8,
      },
    },
  };

  const auto& form =
      prepare(Form::Schema::composite(fields.get_view().slice(0, 2), 16, 8));
  const auto a =
      Encoding::Element::decode(form.get_bytes(), 1, form.get_depth());
  const auto b =
      Encoding::Element::decode(form.get_bytes(), 2, form.get_depth());
  EXPECT_EQ(a.type, b.type);
  EXPECT(a.is_inline());
  EXPECT(b.is_pointer());

  Form::Compiler compiler;
  EXPECT(
      compiler.compile(Form::Schema::Reference(&target.get_abi(), 2), 8) ==
      Status::Invalid);
}

VALIDATION_TEST(Pointers, opaque_spelling) {
  Validation::DataTests::Preparation prepare(8);
  const Storage::Vector<U8, 8> expected = {
    {
      0x11,
      0x80,
      0x80,
      0,
      0x80,
      0x20,
      0,
      1,
    },
  };

  const auto& pointer = prepare(Form::Schema::pointer());
  EXPECT(pointer.compatible(
      Form::Representation(expected.get_data(), sizeof(expected))));
  // Both ways of describing an opaque pointer compile to the same bytes.
  EXPECT(pointer.compatible(prepare(
      Form::Schema::primitive(
          Form::Schema::Value::Pointer, Form::Schema::ByteOrder::Little, 8))));

  const auto integer = Form::Schema::primitive(Form::Schema::Value::U32);
  const auto& typed = prepare(Form::Schema::pointer(&integer));
  EXPECT_NOT(pointer.compatible(typed));
  typed.next(0).visit(
      [&](Form::Representation::Position value) {
        EXPECT(value.get_value() == Form::Schema::Value::Pointer);
        EXPECT_EQ(value.get_extent(), Count(8));
      },

      [&](Status) { EXPECT(False); });
}

// Both allocations describe one recursive shape. Doubling the source graph
// must not double the emitted bodies or change their reference numbering.
VALIDATION_TEST(Pointers, recursive_sharing) {
  Validation::DataTests::Preparation prepare(8);
  auto a =
      Form::Schema::composite(View::Vector<Form::Schema::Position>(), 8, 8);
  auto b =
      Form::Schema::composite(View::Vector<Form::Schema::Position>(), 8, 8);
  const auto pa = Form::Schema::pointer(&a), pb = Form::Schema::pointer(&b);
  const Form::Schema::Position ab(pb, 0), ba(pa, 0);
  a = Form::Schema::composite({ab, 1}, 8, 8);
  b = Form::Schema::composite({ba, 1}, 8, 8);

  const auto& ready = prepare(a);
  EXPECT(ready.compatible(Form::Representation(recursive.get_data(), 8)));

  Count count = 0;
  EXPECT(ready.visit([&](Form::Representation::Position position) {
    ++count;
    EXPECT(position.get_value() == Form::Schema::Value::Pointer);
    return Status::Success;
  }) == Status::Success);

  EXPECT_EQ(count, Count(1));

  // A difference beyond a recursive edge still rejects agreement. Structural
  // sharing must preserve the other fields even when a target repeats.
  const auto integer = Form::Schema::primitive(Form::Schema::Value::U64);
  const Storage::Vector<Form::Schema::Position, 2> different = {
    {
      Form::Schema::Position(pa, 0),
      {
        integer,
        8,
      },
    },
  };

  b = Form::Schema::composite(different.get_view().slice(0, 2), 16, 8);
  EXPECT_NOT(ready.compatible(prepare(a)));
}

VALIDATION_TEST(Pointers, inline_cycle_rejected) {
  auto node =
      Form::Schema::composite(View::Vector<Form::Schema::Position>(), 8, 8);
  const Form::Schema::Position self(node, 0);
  node = Form::Schema::composite({self, 1}, 8, 8);

  Form::Compiler compiler;
  EXPECT(compiler.compile(node, 8) == Status::Invalid);
}

// The callable is a pointer edge even when its argument is its containing
// struct by value. Preparation must not mistake that signature for inline
// containment and must not omit a function with no payload arguments.
VALIDATION_TEST(Pointers, recursive_callable) {
  Validation::DataTests::Preparation prepare(8);
  auto node =
      Form::Schema::composite(View::Vector<Form::Schema::Position>(), 8, 8);
  const Form::Schema::Argument self(node, 1);
  const auto callback = Form::Schema::callable(
      Form::Callable::Convention::SystemVAMD64, {self, 1}, {}, 8);
  const Form::Schema::Position field(callback, 0);
  node = Form::Schema::composite({field, 1}, 8, 8);

  const auto& ready = prepare(node);
  EXPECT_EQ(ready.get_bytes().get_size(), Count(16));
  EXPECT(ready.visit([](Form::Representation::Position position) {
    return position.get_value() == Form::Schema::Value::Pointer
               ? Status::Success
               : Status::Invalid;
  }) == Status::Success);
}

// These pointers name separately authored structs with the same layout. Once
// the compiler recognizes that shared layout, it can describe both pointers
// as one run while preserving their sixteen byte spacing.
VALIDATION_TEST(Pointers, pointer_run) {
  Validation::DataTests::Preparation prepare(8);
  const auto integer = Form::Schema::primitive(Form::Schema::Value::U32);
  const Form::Schema::Position value(integer, 0);
  const auto a = Form::Schema::composite({value, 1}, 4, 4);
  const auto b = Form::Schema::composite({value, 1}, 4, 4);
  const auto pa = Form::Schema::pointer(&a), pb = Form::Schema::pointer(&b);
  const Storage::Vector<Form::Schema::Position, 2> fields = {
    {
      Form::Schema::Position(pa, 0),
      {
        pb,
        16,
      },
    },
  };

  const auto& explicit_fields =
      prepare(Form::Schema::composite(fields.get_view().slice(0, 2), 24, 8));
  const auto& ranged = prepare(Form::Schema::range(pa, 2, 16, 24, 8));
  EXPECT(explicit_fields.compatible(ranged));
  EXPECT_EQ(ranged.get_bytes().get_size(), Count(16));

  const Count coordinate = 16;
  EXPECT(
      ranged.visit(
          View::Vector<Count>(coordinate, 1),
          [&](Form::Representation::Position position) {
            EXPECT_EQ(position.get_offset(), coordinate);
            return Status::Success;
          }) == Status::Success);
}

// A ring with one distinct field needs information to propagate around the
// entire cycle before equality settles. Independent allocation and reversed
// source member enumeration still have to produce exactly the same stream.
VALIDATION_TEST(Pointers, distinct_ring) {
  Validation::DataTests::Preparation prepare(8);
  constexpr Count size = 32;
  Storage::Vector<Storage::Vector<Form::Schema, size>, 2> nodes;
  Storage::Vector<Storage::Vector<Form::Schema::Reference, size>, 2> pointers;
  Storage::Vector<
      Storage::Vector<Storage::Vector<Form::Schema::Position, 2>, size>, 2>
      fields;
  const auto integer = Form::Schema::primitive(Form::Schema::Value::U32);
  const auto real = Form::Schema::primitive(Form::Schema::Value::R32);
  for (Count copy = 0; copy < 2; ++copy) {
    for (Count i = 0; i < size; ++i) {
      pointers[copy][i] = Form::Schema::pointer(&nodes[copy][(i + 1) % size]);
      fields[copy][i][copy] = Form::Schema::Position(pointers[copy][i], 0);
      fields[copy][i][1 - copy] =
          Form::Schema::Position(i + 1 == size ? real : integer, 8);
      nodes[copy][i] = Form::Schema::composite(
          fields[copy][i].get_view().slice(0, 2), 16, 8);
    }
  }

  const auto& first = prepare(nodes[0][0]);
  EXPECT(first.compatible(prepare(nodes[1][0])));
  EXPECT_EQ(first.get_bytes().get_size(), size * 12);
}

static constexpr auto pointer = Form::Schema::pointer();
static constexpr auto scalar =
    Form::Schema::primitive(Form::Schema::Value::U32);
static constexpr auto& narrow_pointer =
    Form::Compiled<pointer, 4>::get_representation();
static constexpr auto& wide_pointer =
    Form::Compiled<pointer, 8>::get_representation();

// The scalar schema must compile to identical canonical bytes for both
// supported pointer widths.
static_assert(Form::Compiled<scalar, 8>::get_representation().compatible(
    Form::Compiled<scalar, 4>::get_representation()));

VALIDATION_TEST(Pointers, pointer_free_width) {
  const Count widths[] = {4, 8};
  const auto& expected = Form::Compiled<scalar, 8>::get_representation();
  for (const auto width : widths) {
    Form::Representation::compile(scalar, width)
        .visit(
            [&](const Form::Representation& representation) {
              EXPECT(representation.get_bytes() == expected.get_bytes());
              EXPECT_EQ(representation.get_extent(), Count(4));
              EXPECT_EQ(representation.get_alignment(), Count(4));
            },

            [&](Status) { EXPECT(False); });
  }
}

// Navigation receives only descriptor bytes. The described pointer target and
// callable signature contain U16 values, but traversal must return the two
// pointer positions in the containing record, using its selected pointer width.
VALIDATION_TEST(Pointers, descriptor_only_navigation) {
  const auto target = Form::Schema::primitive(Form::Schema::Value::U16);
  const Count widths[] = {4, 8};
  for (const auto width : widths) {
    const auto convention = width == 4
                                ? Form::Callable::Convention::EmscriptenWasm32
                                : Form::Callable::Convention::SystemVAMD64;
    const Form::Schema::Argument argument(target, 1);
    const auto callable =
        Form::Schema::callable(convention, {argument, 1}, target, width);
    const Storage::Vector<Form::Schema::Position, 2> fields = {{
      Form::Schema::Position(Form::Schema::pointer(&target), 0),
      {callable, 8},
    }};

    const auto record = Form::Schema::composite(fields.get_view(), 16, 8);

    Form::Representation::compile(record, width)
        .visit(
            [&](const Form::Representation& representation) {
              const Count offsets[] = {0, 8};
              for (const auto offset : offsets) {
                ttx_representation_position position{};
                ASSERT_EQ(
                    ttx_representation_next(
                        &representation.get_abi(), offset, &position),
                    TTX_DATA_SUCCESS);
                EXPECT_EQ(position.offset, offset);
                EXPECT_EQ(position.type, TTX_SCHEMA_POINTER);
                EXPECT_EQ(position.extent, width);
              }

              Count count = 0;
              EXPECT(
                  representation.visit(
                      [&](Form::Representation::Position position) {
                        EXPECT_EQ(position.get_offset(), count * 8);
                        EXPECT(
                            position.get_value() ==
                            Form::Schema::Value::Pointer);
                        EXPECT_EQ(position.get_extent(), width);
                        ++count;
                        return Status::Success;
                      }) == Status::Success);

              EXPECT_EQ(count, Count(2));

              const Storage::Vector<Count, 1> selected = {{8}};
              count = 0;
              EXPECT(
                  representation.visit(
                      selected, [&](Form::Representation::Position position) {
                        EXPECT_EQ(position.get_offset(), Count(8));
                        EXPECT(
                            position.get_value() ==
                            Form::Schema::Value::Pointer);
                        EXPECT_EQ(position.get_extent(), width);
                        ++count;
                        return Status::Success;
                      }) == Status::Success);

              EXPECT_EQ(count, Count(1));
            },

            [&](Status) { EXPECT(False); });
  }
}

VALIDATION_TEST(Pointers, pointer_width) {
  const Storage::Vector<U8, 16> expected = {
    {
      0x10,
      0,
      0,
      0,
      1,
      0,
      0,
      0,
      0x11,
      0x40,
      0x40,
      0,
      0x80,
      0x10,
      0,
      1,
    },
  };

  EXPECT(
      narrow_pointer.get_bytes() ==
      View::Bytes(expected[0], expected.get_size()));
  EXPECT(narrow_pointer.get_pointer_size() == 4);
  EXPECT_EQ(narrow_pointer.get_extent(), Count(4));
  EXPECT_EQ(narrow_pointer.get_alignment(), Count(4));
  EXPECT(!narrow_pointer.compatible(wide_pointer));

  Form::Representation::compile(pointer, 4)
      .visit(
          [&](const Form::Representation& form) {
            EXPECT(form.compatible(narrow_pointer));
          },

          [&](Status) { EXPECT(false); });

  narrow_pointer.next(0).visit(
      [&](Form::Representation::Position position) {
        EXPECT_EQ(position.get_extent(), Count(4));
        EXPECT(position.get_value() == Form::Schema::Value::Pointer);
      },

      [&](Status) { EXPECT(false); });

  const Storage::Vector<Count, 1> coordinates;
  Count visits = 0;
  EXPECT(
      narrow_pointer.visit(
          coordinates, [&](Form::Representation::Position position) {
            EXPECT_EQ(position.get_extent(), Count(4));
            ++visits;
            return Status::Success;
          }) == Status::Success);

  EXPECT_EQ(visits, Count(1));
}

// The C compilation entry receives a runtime width. Reject unsupported storage
// there as well as in the ordinary C++ runtime entry.
VALIDATION_TEST(Pointers, unsupported_width) {
  Form::Compiler compiler;
  EXPECT(compiler.compile(pointer, 3) == Status::Unsupported);

  Form::Representation::compile(pointer, 3)
      .visit(
          [&](const Form::Representation& representation) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Unsupported); });
}

VALIDATION_TEST(Pointers, identical_geometry) {
  // Both arrays have the same starts, enclosing extent and alignment. The
  // pointer width still distinguishes four byte pointers with gaps from eight
  // byte pointers, even though their ordinary descriptor blocks are identical.
  static constexpr auto array = Form::Schema::range(pointer, 2, 8, 16, 8);
  const auto& narrow = Form::Compiled<array, 4>::get_representation();
  const auto& wide = Form::Compiled<array, 8>::get_representation();
  EXPECT(narrow.get_blocks() == wide.get_blocks());
  EXPECT(!narrow.compatible(wide));
}

VALIDATION_TEST(Pointers, convention_agreement) {
  const auto function = Form::Schema::callable(
      Form::Callable::Convention::EmscriptenWasm32, {}, scalar, 4);
  Form::Compiler compiler;
  EXPECT(compiler.compile(function, 4) == Status::Success);

  auto wrong = function.get_abi();
  wrong.data.callable.convention = TTX_SCHEMA_SYSTEM_V_AMD64;
  EXPECT(compiler.compile(wrong, 4) == Status::Invalid);
}

VALIDATION_TEST(Pointers, composed_pointers) {
  const Storage::Vector<Form::Representation::Member, 2> members = {
    {
      Form::Representation::Member(&narrow_pointer.get_abi(), 0),
      {
        &narrow_pointer.get_abi(),
        4,
      },
    },
  };

  Form::Representation::compose(members, 8, 4)
      .visit(
          [&](const Form::Representation& form) {
            EXPECT(form.get_pointer_size() == 4);

            Count count = 0;
            EXPECT(form.visit([&](Form::Representation::Position position) {
              EXPECT_EQ(position.get_extent(), Count(4));
              ++count;
              return Status::Success;
            }) == Status::Success);

            EXPECT_EQ(count, Count(2));
          },

          [&](Status) { EXPECT(false); });

  const Storage::Vector<Form::Representation::Member, 2> mixed = {
    {
      Form::Representation::Member(&narrow_pointer.get_abi(), 0),
      {
        &wide_pointer.get_abi(),
        8,
      },
    },
  };

  Form::Representation::compose(mixed, 16, 8)
      .visit(
          [&](const Form::Representation& representation) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Incompatible); });
}
