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

#endif
