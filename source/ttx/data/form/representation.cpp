// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/representation.hpp"

#include "perimortem/core/data.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Ttx::Data;

// Byte lookup can skip padding and whole instances. A coordinate inside a
// primitive asks for the next start, while an exact coordinate selects that
// primitive. Whole traversal uses visit instead of repeating this search.
static auto next(
    const Form::Representation& source,
    Count body,
    Count offset,
    Count requested,
    Form::Representation::Position& result) -> Bool {
  const auto bytes = source.get_blocks();
  const U8 depth = source.get_depth();
  const auto form = Encoding::Struct::decode(bytes, body, depth);
  for (Count i = 0; i < form.count; ++i) {
    const auto entry = Encoding::Element::decode(bytes, body + i + 1, depth);
    const Count first = offset + entry.offset;
    const Count local = requested > first ? requested - first : 0;
    Count instance = local / entry.distance;
    if (instance < entry.count) {
      const Count start = first + instance * entry.distance;
      if (entry.is_inline()) {
        if (next(source, entry.type, start, requested, result)) {
          return True;
        }

        ++instance;
        if (instance < entry.count) {
          return next(
              source, entry.type, first + instance * entry.distance, 0, result);
        }
      } else {
        instance += start < requested;
        if (instance < entry.count) {
          result = Form::Representation::Position(
              first + instance * entry.distance, entry.get_value(),
              entry.get_byte_order(),
              U32(entry.get_extent(source.get_pointer_size())));
          return True;
        }
      }
    }
  }

  return False;
}

auto ttx_representation_compatible(
    const ttx_representation* source,
    const ttx_representation* destination) -> U8 {
  return source && destination &&
         Form::Representation(*source).compatible(*destination);
}

auto ttx_representation_next(
    const ttx_representation* source,
    Count offset,
    ttx_representation_position* result) -> ttx_data_status {
  if (!source || !result) {
    return TTX_DATA_INVALID;
  }

  Form::Representation::Position position;
  if (!next(*source, 0, 0, offset, position)) {
    return TTX_DATA_BOUNDS;
  }

  *result = position.get_abi();
  return TTX_DATA_SUCCESS;
}

auto Form::Representation::next(Count offset) const
    -> Storage::Result<Position, Status> {
  ttx_representation_position result;
  const auto status = ttx_representation_next(&value, offset, &result);
  if (status != TTX_DATA_SUCCESS) {
    return Status(status);
  }

  return Position(result);
}

auto ttx_representation_visit(
    const ttx_representation* source,
    ttx_representation_visitor visitor) -> ttx_data_status {
  if (!source || !visitor.visit) {
    return TTX_DATA_INVALID;
  }

  return ttx_data_status(
      Form::Representation(*source).visit(
          [&](Form::Representation::Position position) {
            return Status(visitor.visit(visitor.context, position.get_abi()));
          }));
}

auto ttx_representation_visit_selected(
    const ttx_representation* source,
    const Count* coordinates,
    Count count,
    ttx_representation_visitor visitor) -> ttx_data_status {
  if (!source || !visitor.visit || (count && !coordinates)) {
    return TTX_DATA_INVALID;
  }

  const auto coordinate_view = count
                                   ? View::Vector<Count>(coordinates[0], count)
                                   : View::Vector<Count>();
  return ttx_data_status(
      Form::Representation(*source).visit(
          coordinate_view, [&](Form::Representation::Position position) {
            return Status(visitor.visit(visitor.context, position.get_abi()));
          }));
}

static auto allocate_representation(void* context, Count bytes, Count alignment)
    -> void* {
  auto& arena = *Data::cast<Allocator::Arena>(context);
  return arena.allocate(bytes).get_data();
}

auto Form::Representation::compile(
    ttx_schema_reference schema,
    Allocator::Arena& arena,
    Count pointer_size) -> Storage::Result<Form::Representation, Status> {
  const ttx_representation_allocator allocator =
      ttx_representation_allocator(&arena, allocate_representation);
  const ttx_representation* result = nullptr;
  const auto status =
      ttx_representation_compile(schema, pointer_size, allocator, &result);
  if (status != TTX_DATA_SUCCESS) {
    return Status(status);
  }

  return Form::Representation(*result);
}

auto Form::Representation::compose(
    View::Vector<Member> members,
    Count extent,
    Count alignment,
    Allocator::Arena& arena) -> Storage::Result<Form::Representation, Status> {
  const ttx_representation_allocator allocator =
      ttx_representation_allocator(&arena, allocate_representation);
  const ttx_representation* result = nullptr;
  const auto status = ttx_representation_compose(
      members.get_data(), members.get_size(), extent, alignment, allocator,
      &result);
  if (status != TTX_DATA_SUCCESS) {
    return Status(status);
  }

  return Form::Representation(*result);
}
