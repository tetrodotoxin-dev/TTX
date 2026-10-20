// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "ttx/data/form/representation.h"

using namespace Toolchain::Validation;

static Harness Records = {.name = "TTX::Data::Form::Records"};

// This translation unit includes only the C form headers. C++ aggregate
// initialization constructs the records, and the exported C functions compile
// and navigate them. Linking this test checks that record construction needs
// only the declarations provided by those headers.
static_assert(__is_aggregate(ttx_schema));
static_assert(__is_trivial(ttx_schema));
static_assert(__is_aggregate(ttx_representation));
static_assert(__is_trivial(ttx_representation_position));

VALIDATION_TEST(Records, c_headers_in_cpp) {
  const ttx_schema integer = {
    4,
    4,
    TTX_SCHEMA_VALUE,
    {.value = {TTX_SCHEMA_U32, TTX_SCHEMA_LITTLE_ENDIAN}}};

  const ttx_schema_position positions[] = {
    {{&integer, 0}, 0}, {{&integer, 0}, 4}};

  const ttx_schema record = {
    8, 4, TTX_SCHEMA_COMPOSITE, {.composite = {positions, 2}}};

  alignas(8) U8 storage[128] = {};

  const ttx_representation_allocator allocator = {
    storage, [](void* context, Count bytes, Count alignment) -> void* {
      return bytes <= 128 && alignment <= 8 ? context : nullptr;
    }};

  const ttx_representation* representation = nullptr;
  ASSERT_EQ(
      ttx_representation_compile(
          {&record, 0}, sizeof(void*), allocator, &representation),
      TTX_DATA_SUCCESS);

  ttx_representation_position position = {};
  ASSERT_EQ(
      ttx_representation_next(representation, 1, &position), TTX_DATA_SUCCESS);
  EXPECT_EQ(position.offset, Count(4));
  EXPECT_EQ(position.type, TTX_SCHEMA_U32);
  EXPECT_EQ(position.extent, U32(4));
}
