// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include <stddef.h>

#include "ttx/data/transfer.h"

#if __has_include(                         \
    "ttx/semantic/negotiation/query.h") || \
    __has_include("ttx/graph/abstract.h")
#error The Data component exposed a higher layer.
#endif

typedef struct output_storage {
  _Alignas(8) U8 bytes[256];
} output_storage;

static void* allocate(void* context, Count size, Count alignment) {
  output_storage* storage = context;
  const CppSize start = (CppSize)storage->bytes;
  const CppSize aligned = (start + alignment - 1) & ~(alignment - 1);
  return aligned + size <= start + sizeof(storage->bytes) ? (void*)aligned
                                                          : NULL;
}

S32 main(void) {
  static const ttx_schema integer = {
    4,
    4,
    TTX_SCHEMA_VALUE,
    {
      .value =
          {
            TTX_SCHEMA_U32,
            TTX_SCHEMA_LITTLE_ENDIAN,
          },
    },
  };

  output_storage encoded = {0};
  const ttx_representation* representation = NULL;
  const ttx_representation_allocator allocator = {&encoded, allocate};
  if (ttx_representation_compile(
          (ttx_schema_reference){&integer, 0}, sizeof(void*), allocator,
          &representation) != TTX_DATA_SUCCESS) {
    return 1;
  }

  U32 value = 42;
  const ttx_storage input = {representation, (U8*)&value, sizeof(value)};
  U32 output = 0;
  const ttx_storage target = {
    representation,
    (U8*)&output,
    sizeof(output),
  };

  if (ttx_transfer(&input, &target) != TTX_DATA_SUCCESS) {
    return 2;
  }

  return output == 42 ? 0 : 3;
}
