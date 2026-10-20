// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/form/callable.hpp"

namespace Ttx::Data::Form {

// Providers need to describe their data representation without deciding how
// every consumer will navigate it. Schema supplies that source vocabulary,
// including explicit offsets, repetition distances and padding. Compilation
// establishes the concrete geometry that transports borrow through a
// Representation. The source can then be discarded independently of that
// result.
//
// Composite preserves a real object boundary around its physical elements.
// Range describes repetition with an element, count and distance. A repeated
// scalar therefore compiles to one run even when it describes many values.
//
// Names and the choice of which source supplies an output belong to the
// semantic layer. Source descriptions may be runtime objects or C++ constants.
// They need only remain stable during compilation. The resulting canonical form
// has its own lifetime and contains no source identity used to justify
// compatibility.
//
// Schema stores one C record. Positions and arguments remain arrays of C
// records, and references point to those records rather than C++ interfaces.
// Keep a referenced Schema at its original address through compilation.
class Schema {
 public:
  // Kind selects the target's description. Reference flags independently
  // choose pointer storage, so a struct definition can serve both uses.
  enum class Kind : U8 {
    Value = TTX_SCHEMA_VALUE,
    Composite = TTX_SCHEMA_COMPOSITE,
    Range = TTX_SCHEMA_RANGE,
    Callable = TTX_SCHEMA_CALLABLE,
  };

  enum class Value : U8 {
    U8 = TTX_SCHEMA_U8,
    U16 = TTX_SCHEMA_U16,
    U32 = TTX_SCHEMA_U32,
    U64 = TTX_SCHEMA_U64,
    S8 = TTX_SCHEMA_S8,
    S16 = TTX_SCHEMA_S16,
    S32 = TTX_SCHEMA_S32,
    S64 = TTX_SCHEMA_S64,
    R32 = TTX_SCHEMA_R32,
    R64 = TTX_SCHEMA_R64,
    Pointer = TTX_SCHEMA_POINTER,
    V64 = TTX_SCHEMA_V64,
    V128 = TTX_SCHEMA_V128,
    V256 = TTX_SCHEMA_V256,
    V512 = TTX_SCHEMA_V512,
  };

  enum class ByteOrder : U8 {
    Little = TTX_SCHEMA_LITTLE_ENDIAN,
    Big = TTX_SCHEMA_BIG_ENDIAN
  };

  // Reference chooses inline storage or a pointer to a described target.
  // It borrows the target C record so C and C++ can describe the same graph.
  // An unset reference denotes void when used as a callable result.
  class Reference {
   public:
    constexpr Reference(ttx_schema_reference value = {}) : value(value) {}
    constexpr Reference(const ttx_schema* schema, U8 flags = 0)
        : value{schema, flags} {}
    constexpr Reference(const ttx_schema& schema) : value{&schema, 0} {}
    constexpr Reference(const Schema& schema);

    constexpr operator ttx_schema_reference() const { return value; }
    constexpr auto get_abi() const -> ttx_schema_reference { return value; }
    constexpr auto is_pointer() const -> Bool {
      return value.flags & TTX_SCHEMA_REFERENCE_POINTER;
    }

    constexpr auto is_set() const -> Bool {
      return value.schema || is_pointer();
    }

    constexpr auto get_extent(Count pointer_size = sizeof(void*)) const
        -> Count;
    constexpr auto get_alignment(Count pointer_size = sizeof(void*)) const
        -> Count;

   private:
    ttx_schema_reference value;
  };

  using Position = ttx_schema_position;
  using Argument = ttx_schema_argument;
  using V64 = ttx_vector64;
  using V128 = ttx_vector128;
  using V256 = ttx_vector256;
  using V512 = ttx_vector512;

  constexpr Schema(ttx_schema value = {}) : value(value) {}
  constexpr operator ttx_schema_reference() const { return {&value, 0}; }
  constexpr auto get_abi() const -> const ttx_schema& { return value; }

  static constexpr auto get_width(
      Value type,
      Count pointer_size = sizeof(void*)) -> Count;
  static constexpr auto primitive(
      Value type,
      ByteOrder order = ByteOrder::Little,
      Count pointer_size = sizeof(void*)) -> Schema;
  static constexpr auto pointer(const Schema* target = nullptr) -> Reference {
    return Reference(
        target ? &target->value : nullptr, TTX_SCHEMA_REFERENCE_POINTER);
  }

  static constexpr auto callable(
      Callable::Convention convention,
      Perimortem::Core::View::Vector<Argument> arguments,
      Reference result = {},
      Count pointer_size = sizeof(void*)) -> Schema;
  static constexpr auto composite(
      Perimortem::Core::View::Vector<Position> positions,
      Count extent,
      Count alignment = 1) -> Schema;
  static constexpr auto range(
      Reference element,
      Count repeats,
      Count distance,
      Count extent,
      Count alignment = 1) -> Schema;

  constexpr auto get_extent() const -> Count { return value.extent; }
  constexpr auto get_alignment() const -> Count { return value.alignment; }
  constexpr auto get_kind() const -> Kind { return Kind(value.kind); }
  constexpr auto get_value() const -> Value {
    return Value(value.data.value.type);
  }

  constexpr auto get_byte_order() const -> ByteOrder {
    return ByteOrder(value.data.value.byte_order);
  }

  // Read the union member selected by Kind. An empty position View represents
  // a null pointer or zero count. The compiler checks the C record's original
  // pointer and count first so malformed arrays remain distinguishable.
  constexpr auto get_positions() const
      -> Perimortem::Core::View::Vector<Position> {
    if (!value.data.composite.positions || !value.data.composite.count) {
      return {};
    }

    return {value.data.composite.positions[0], value.data.composite.count};
  }

  constexpr auto get_range() const -> ttx_schema_range {
    return value.data.range;
  }

  constexpr auto get_callable() const -> Callable {
    return Callable(value.data.callable);
  }

 private:
  ttx_schema value;
};

constexpr Schema::Reference::Reference(const Schema& schema)
    : value{&schema.get_abi(), 0} {}

constexpr auto Schema::get_width(Value type, Count pointer_size) -> Count {
  switch (type) {
  case Value::U8:
  case Value::S8:
    return 1;
  case Value::U16:
  case Value::S16:
    return 2;
  case Value::U32:
  case Value::S32:
  case Value::R32:
    return 4;
  case Value::U64:
  case Value::S64:
  case Value::R64:
  case Value::V64:
    return 8;
  case Value::Pointer:
    return pointer_size;
  case Value::V128:
    return 16;
  case Value::V256:
    return 32;
  case Value::V512:
    return 64;
  }

  return 0;
}

constexpr auto Schema::primitive(
    Value type,
    ByteOrder order,
    Count pointer_size) -> Schema {
  const Count width = get_width(type, pointer_size);
  return Schema(ttx_schema(
      width, width, U8(Kind::Value),
      {
        .value =
            {
              U8(type),
              U8(order),
            },
      }));
}

constexpr auto Schema::callable(
    Callable::Convention convention,
    Perimortem::Core::View::Vector<Argument> arguments,
    Reference result,
    Count pointer_size) -> Schema {
  return Schema(ttx_schema(
      pointer_size, pointer_size, U8(Kind::Callable),
      {
        .callable =
            {
              arguments.get_data(),
              arguments.get_size(),
              result,
              ttx_schema_convention(convention),
            },
      }));
}

constexpr auto Schema::composite(
    Perimortem::Core::View::Vector<Position> positions,
    Count extent,
    Count alignment) -> Schema {
  return Schema(ttx_schema(
      extent, alignment, U8(Kind::Composite),
      {
        .composite =
            {
              positions.get_data(),
              positions.get_size(),
            },
      }));
}

constexpr auto Schema::range(
    Reference element,
    Count repeats,
    Count distance,
    Count extent,
    Count alignment) -> Schema {
  return Schema(ttx_schema(
      extent, alignment, U8(Kind::Range),
      {
        .range =
            {
              element,
              repeats,
              distance,
            },
      }));
}

constexpr auto Schema::Reference::get_extent(Count pointer_size) const
    -> Count {
  if (is_pointer() ||
      (value.schema && value.schema->kind == TTX_SCHEMA_CALLABLE)) {
    return pointer_size;
  }

  return value.schema ? value.schema->extent : 0;
}

constexpr auto Schema::Reference::get_alignment(Count pointer_size) const
    -> Count {
  if (is_pointer() ||
      (value.schema && value.schema->kind == TTX_SCHEMA_CALLABLE)) {
    return pointer_size;
  }

  return value.schema ? value.schema->alignment : 1;
}

}  // namespace Ttx::Data::Form
