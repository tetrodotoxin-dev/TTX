// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_FORM_SCHEMA_H
#define TTX_DATA_FORM_SCHEMA_H

#include "ttx/data/status.h"

// TTX requires data and function pointers to have the same size and alignment:
// either four bytes or eight bytes. Native schemas use the host's pointer size.
// When compiling a schema for another target, the caller can select that
// target's pointer size explicitly.
#ifdef __cplusplus

static_assert(sizeof(void*) == 4 || sizeof(void*) == 8);
static_assert(alignof(void*) == sizeof(void*));
static_assert(sizeof(void (*)(void)) == sizeof(void*));
static_assert(alignof(void (*)(void)) == sizeof(void*));

#else

_Static_assert(
    (sizeof(void*) == 4 || sizeof(void*) == 8) &&
        _Alignof(void*) == sizeof(void*) &&
        sizeof(void (*)(void)) == sizeof(void*) &&
        _Alignof(void (*)(void)) == sizeof(void*),
    "Unsupported native pointer storage");

#endif

#define TTX_SCHEMA_VALUE ((U8)1)
#define TTX_SCHEMA_COMPOSITE ((U8)2)
#define TTX_SCHEMA_RANGE ((U8)4)
#define TTX_SCHEMA_CALLABLE ((U8)5)

typedef U8 ttx_schema_value;

#define TTX_SCHEMA_U8 ((ttx_schema_value)1)
#define TTX_SCHEMA_U16 ((ttx_schema_value)2)
#define TTX_SCHEMA_U32 ((ttx_schema_value)3)
#define TTX_SCHEMA_U64 ((ttx_schema_value)4)
#define TTX_SCHEMA_S8 ((ttx_schema_value)5)
#define TTX_SCHEMA_S16 ((ttx_schema_value)6)
#define TTX_SCHEMA_S32 ((ttx_schema_value)7)
#define TTX_SCHEMA_S64 ((ttx_schema_value)8)
#define TTX_SCHEMA_R32 ((ttx_schema_value)9)
#define TTX_SCHEMA_R64 ((ttx_schema_value)10)
#define TTX_SCHEMA_POINTER ((ttx_schema_value)12)
#define TTX_SCHEMA_V64 ((ttx_schema_value)13)
#define TTX_SCHEMA_V128 ((ttx_schema_value)14)
#define TTX_SCHEMA_V256 ((ttx_schema_value)15)
#define TTX_SCHEMA_V512 ((ttx_schema_value)16)
#define TTX_SCHEMA_LITTLE_ENDIAN ((U8)0)
#define TTX_SCHEMA_BIG_ENDIAN ((U8)1)

// Vector observations use fixed, aligned bit carriers. Returning these through
// an output pointer permits access without compiling that caller for
// AVX or AVX512. Native callables use the corresponding SIMD register ABI.
// Lane interpretation belongs to the operation using those bits. Big endian
// storage reverses the whole carrier, just as for another primitive bit value.
#ifdef __cplusplus
#define TTX_VECTOR_ALIGNMENT(size) alignas(size)
#else
#define TTX_VECTOR_ALIGNMENT(size) _Alignas(size)
#endif

typedef struct ttx_vector64 {
  TTX_VECTOR_ALIGNMENT(8) U8 bytes[8];
} ttx_vector64;

typedef struct ttx_vector128 {
  TTX_VECTOR_ALIGNMENT(16) U8 bytes[16];
} ttx_vector128;

typedef struct ttx_vector256 {
  TTX_VECTOR_ALIGNMENT(32) U8 bytes[32];
} ttx_vector256;

typedef struct ttx_vector512 {
  TTX_VECTOR_ALIGNMENT(64) U8 bytes[64];
} ttx_vector512;

#undef TTX_VECTOR_ALIGNMENT

// A reference chooses whether a described target is stored inline or through
// a pointer. Keeping that choice on the edge lets one struct or callable
// description be reused without manufacturing a second Schema object.
#define TTX_SCHEMA_REFERENCE_POINTER ((U8)1)

typedef struct ttx_schema_reference {
  const struct ttx_schema* schema;
  U8 flags;

} ttx_schema_reference;

typedef U32 ttx_schema_convention;

#define TTX_SCHEMA_SYSTEM_V_AMD64 ((ttx_schema_convention)1)
#define TTX_SCHEMA_SYSTEM_V_AMD64_VARIADIC ((ttx_schema_convention)2)
#define TTX_SCHEMA_EMSCRIPTEN_WASM32 ((ttx_schema_convention)3)
#define TTX_SCHEMA_EMSCRIPTEN_WASM32_VARIADIC ((ttx_schema_convention)4)
#define TTX_SCHEMA_WINDOWS_X64 ((ttx_schema_convention)5)
#define TTX_SCHEMA_WINDOWS_X64_VARIADIC ((ttx_schema_convention)6)
#if defined(_WIN64)
#define TTX_SCHEMA_NATIVE_CONVENTION TTX_SCHEMA_WINDOWS_X64
#define TTX_SCHEMA_NATIVE_VARIADIC TTX_SCHEMA_WINDOWS_X64_VARIADIC
#elif defined(__EMSCRIPTEN__)
#define TTX_SCHEMA_NATIVE_CONVENTION TTX_SCHEMA_EMSCRIPTEN_WASM32
#define TTX_SCHEMA_NATIVE_VARIADIC TTX_SCHEMA_EMSCRIPTEN_WASM32_VARIADIC
#else
#define TTX_SCHEMA_NATIVE_CONVENTION TTX_SCHEMA_SYSTEM_V_AMD64
#define TTX_SCHEMA_NATIVE_VARIADIC TTX_SCHEMA_SYSTEM_V_AMD64_VARIADIC
#endif

// An argument entry can describe consecutive identical formal parameters with
// one count. Their declaration order is retained, and compilation combines
// adjacent equal entries without turning those parameters into an array.
// A variadic signature describes only its fixed prefix through this list.
typedef struct ttx_schema_argument {
  ttx_schema_reference reference;
  Count count;

} ttx_schema_argument;

// An unset result reference denotes void. A context passed to the function is
// an ordinary explicit argument. These facts describe the realized C call
// boundary and impose no native ownership or language object model on its
// provider.
typedef struct ttx_schema_callable {
  const ttx_schema_argument* arguments;
  Count count;
  ttx_schema_reference result;
  ttx_schema_convention convention;

} ttx_schema_callable;

// Red and green can use the same U32 schema but occupy different positions in
// a color. Their placement belongs to the parent's entries, which lets a child
// description be reused without changing its meaning. Each placement supplies
// the byte offset from the start of the described record.
typedef struct ttx_schema_position {
  ttx_schema_reference reference;
  Count offset;

} ttx_schema_position;

typedef struct ttx_schema_composite {
  const ttx_schema_position* positions;
  Count count;
} ttx_schema_composite;

typedef struct ttx_schema_range {
  ttx_schema_reference element;
  Count count;
  Count distance;

} ttx_schema_range;

typedef struct ttx_schema_primitive {
  U8 type;
  U8 byte_order;
} ttx_schema_primitive;

// Schema describes data layouts and callable signatures for compilation.
// Field offsets, repetition distances and padding describe where values are
// stored. Compilation produces a canonical form that Representation can
// compare and navigate independently of the source Schema.
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
typedef struct ttx_schema {
  Count extent;
  Count alignment;
  U8 kind;
  union {

    ttx_schema_primitive value;

    ttx_schema_composite composite;
    ttx_schema_range range;
    ttx_schema_callable callable;
  } data;

} ttx_schema;

#ifdef __cplusplus

#include "perimortem/core/view/vector.hpp"

namespace Ttx::Data::Form {

// Callable describes a function's calling convention, result and ordered
// arguments. An unset result denotes void. Argument entries can repeat a
// parameter type, while a variadic signature describes only its fixed prefix.
// The argument records and their referenced schemas remain available and
// unchanged through compilation. Copying Callable copies the C record and
// continues to reference those same arrays.
class Callable {
 public:
  enum class Convention : U32 {
    SystemVAMD64 = TTX_SCHEMA_SYSTEM_V_AMD64,
    SystemVAMD64Variadic = TTX_SCHEMA_SYSTEM_V_AMD64_VARIADIC,
    EmscriptenWasm32 = TTX_SCHEMA_EMSCRIPTEN_WASM32,
    EmscriptenWasm32Variadic = TTX_SCHEMA_EMSCRIPTEN_WASM32_VARIADIC,
    WindowsX64 = TTX_SCHEMA_WINDOWS_X64,
    WindowsX64Variadic = TTX_SCHEMA_WINDOWS_X64_VARIADIC,
    Native = TTX_SCHEMA_NATIVE_CONVENTION,
    NativeVariadic = TTX_SCHEMA_NATIVE_VARIADIC,
  };

  constexpr Callable(ttx_schema_callable value) : value(value) {}

  constexpr auto get_arguments() const
      -> Perimortem::Core::View::Vector<ttx_schema_argument> {
    if (!value.arguments || !value.count) {
      return {};
    }

    return {value.arguments[0], value.count};
  }

  constexpr auto get_result() const -> ttx_schema_reference {
    return value.result;
  }

  constexpr auto get_convention() const -> Convention {
    return Convention(value.convention);
  }

  constexpr auto get_abi() const -> const ttx_schema_callable& { return value; }

 private:
  ttx_schema_callable value;
};

}  // namespace Ttx::Data::Form

namespace Ttx::Data::Form {

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
  // The target is a C record, so C and C++ can describe the same graph.
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

#endif

#endif
