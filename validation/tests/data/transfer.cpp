// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/transfer.hpp"

#include <string.h>

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Data;

static Harness Transfers = {.name = "TTX::Data::Transfer"};

static constexpr auto integer =
    Form::Schema::primitive(Form::Schema::Value::U32);
static constexpr auto four = Form::Schema::range(integer, 4, 4, 16, 4);
static constexpr auto spaced = Form::Schema::range(integer, 2, 256, 260, 4);

// Moving canonical descriptor bytes preserves their meaning at every byte
// alignment. Payload addresses still satisfy the alignment described by them.
VALIDATION_TEST(Transfers, unaligned_descriptors) {
  const Form::Representation* forms[] = {
    &Form::Compiled<four>::get_representation(),
    &Form::Compiled<spaced>::get_representation()};

  constexpr Count descriptor_size =
      Form::Compiled<spaced>::get_representation().size;
  alignas(8) U8 source_bytes[descriptor_size + 7] = {};
  alignas(8) U8 target_bytes[descriptor_size + 7] = {};
  alignas(U32) U8 input[260] = {};
  alignas(U32) U8 output[261] = {};
  memset(input, 0x3c, sizeof(input));

  for (Count index = 0; index != 2; ++index) {
    const auto& canonical = *forms[index];
    const Count extent = index ? 260 : 16;
    const Count distance = index ? 256 : 4;
    const Count count = index ? 2 : 4;
    ASSERT(canonical.size <= descriptor_size);
    EXPECT_EQ(canonical.get_depth(), U8(index + 1));

    for (Count offset = 1; offset != 8; ++offset) {
      memcpy(source_bytes + offset, canonical.data, canonical.size);
      memcpy(target_bytes + 8 - offset, canonical.data, canonical.size);

      const Form::Representation source(source_bytes + offset, canonical.size);
      const Form::Representation target(
          target_bytes + 8 - offset, canonical.size);
      EXPECT(ttx_representation_compatible(&canonical, &source));
      EXPECT(ttx_representation_compatible(&source, &target));
      EXPECT_EQ(source.get_extent(), extent);
      EXPECT_EQ(source.get_alignment(), Count(4));

      ttx_representation_position position;
      ASSERT_EQ(
          ttx_representation_next(&source, 1, &position), TTX_DATA_SUCCESS);
      EXPECT_EQ(position.offset, distance);
      EXPECT(position.get_value() == Form::Schema::Value::U32);

      Count visited = 0;
      EXPECT(source.visit([&](Form::Representation::Position current) {
        EXPECT_EQ(current.offset, visited * distance);
        EXPECT_EQ(current.get_extent(), Count(4));
        ++visited;
        return Status::Success;
      }) == Status::Success);

      EXPECT_EQ(visited, count);

      const Count last = (count - 1) * distance;
      visited = 0;
      EXPECT(
          source.visit(
              View::Vector<Count>(last, 1),
              [&](Form::Representation::Position current) {
                EXPECT_EQ(current.offset, last);
                ++visited;
                return Status::Success;
              }) == Status::Success);

      EXPECT_EQ(visited, Count(1));

      memset(output, 0xa5, sizeof(output));

      const ttx_storage from{&source, input, sizeof(input)};
      const ttx_storage to{&target, output, sizeof(output)};
      ASSERT_EQ(ttx_transfer(&from, &to), TTX_DATA_SUCCESS);
      EXPECT_EQ(memcmp(input, output, extent), 0);
      EXPECT_EQ(output[extent], U8(0xa5));

      memset(output, 0xa5, sizeof(output));

      const ttx_storage misaligned{&target, output + 1, sizeof(output) - 1};
      EXPECT_EQ(ttx_transfer(&from, &misaligned), TTX_DATA_INVALID);
      for (const U8 byte : output) {
        EXPECT_EQ(byte, U8(0xa5));
      }
    }
  }
}

VALIDATION_TEST(Transfers, complete_extent_and_overlap) {
  const auto& form = Form::Compiled<four>::get_representation();
  U32 values[] = {1, 2, 3, 4, 5};
  const ttx_storage input{&form, Data::cast<U8>(values), sizeof(values)};
  const ttx_storage output{&form, Data::cast<U8>(values + 1), 16};
  ASSERT_EQ(ttx_transfer(&input, &output), TTX_DATA_SUCCESS);

  const U32 expected[] = {1, 1, 2, 3, 4};
  for (Count index = 0; index != 5; ++index) {
    EXPECT_EQ(values[index], expected[index]);
  }

  EXPECT_EQ(ttx_transfer(&output, &output), TTX_DATA_SUCCESS);

  U32 destination[] = {90, 90, 90, 90, 90, 90};
  const ttx_storage spare{&form, Data::cast<U8>(destination + 1), 20};
  ASSERT_EQ(ttx_transfer(&output, &spare), TTX_DATA_SUCCESS);
  EXPECT_EQ(destination[0], U32(90));
  EXPECT_EQ(destination[5], U32(90));
  EXPECT_EQ(destination[1], U32(1));
  EXPECT_EQ(destination[4], U32(4));
}

// The source and destination are checked independently. Representation
// disagreement is detected while reading descriptor bytes, before interpreting
// the foreign form or modifying any output.
VALIDATION_TEST(Transfers, refused_regions_and_forms) {
  U32 value = 42, output = 99;
  const auto input = Form::Storage::from(value).get_abi();
  const auto target = Form::Storage::from(output).get_abi();
  alignas(U32) U8 unaligned[sizeof(U32) + 1] = {};

  const ttx_storage invalid[] = {
    {input.representation, input.data, sizeof(U32) - 1},
    {input.representation, unaligned + 1, sizeof(U32)},
    {input.representation, nullptr, sizeof(U32)},
    {nullptr, input.data, sizeof(U32)}};

  const ttx_data_status statuses[] = {
    TTX_DATA_BOUNDS, TTX_DATA_INVALID, TTX_DATA_INVALID, TTX_DATA_INVALID};

  for (Count index = 0; index != 4; ++index) {
    EXPECT_EQ(ttx_transfer(&invalid[index], &target), statuses[index]);
    EXPECT_EQ(ttx_transfer(&input, &invalid[index]), statuses[index]);
    EXPECT_EQ(output, U32(99));
  }

  EXPECT_EQ(ttx_transfer(nullptr, &target), TTX_DATA_INVALID);
  EXPECT_EQ(ttx_transfer(&input, nullptr), TTX_DATA_INVALID);

  R32 real = 0;
  const auto different = Form::Storage::from(real).get_abi();
  EXPECT_EQ(ttx_transfer(&different, &target), TTX_DATA_INCOMPATIBLE);

  const U8 malformed[] = {0xff};
  const ttx_representation malformed_form{malformed, sizeof(malformed)};
  const ttx_storage foreign{&malformed_form, input.data, input.size};
  EXPECT_EQ(ttx_transfer(&foreign, &target), TTX_DATA_INCOMPATIBLE);
  EXPECT_EQ(output, U32(99));
}

VALIDATION_TEST(Transfers, empty_value) {
  static constexpr auto empty =
      Form::Schema::composite(View::Vector<Form::Schema::Position>(), 0);
  const auto& form = Form::Compiled<empty>::get_representation();
  const ttx_storage input{&form, nullptr, 0};
  const ttx_storage output{&form, nullptr, 0};
  EXPECT_EQ(ttx_transfer(&input, &output), TTX_DATA_SUCCESS);
}

struct DataOperation {
  void* context;
  U32 (*call)(void*, U32);
};

TTX_DATA_RECORD(
    DataOperation,
    TTX_DATA_MEMBER(DataOperation, context),
    TTX_DATA_MEMBER(DataOperation, call));

VALIDATION_TEST(Transfers, callable_record_without_query) {
  struct State {
    U32 calls = 0;
    static U32 call(void* context, U32 value) {
      ++Data::cast<State>(context)->calls;
      return value + 1;
    }
  } state;

  DataOperation input{&state, State::call}, output{};
  EXPECT(
      Transfer::copy(Form::Storage::from(input), Form::Storage::from(output)) ==
      Status::Success);
  EXPECT_EQ(state.calls, U32(0));
  ASSERT(output.call);
  EXPECT_EQ(output.call(output.context, 41), U32(42));
  EXPECT_EQ(state.calls, U32(1));
}
