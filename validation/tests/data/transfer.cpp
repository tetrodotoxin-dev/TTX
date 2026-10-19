// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/transfer.hpp"

#include <string.h>

#include "toolchain/validation/test.hpp"

using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static Toolchain::Validation::Harness Transfers = {
  .name = "TTX::Data::Transfer"};

static constexpr auto integer = Schema::primitive(Schema::Value::U32);
static constexpr auto four = Schema::range(integer, 4, 4, 16, 4);

VALIDATION_TEST(Transfers, complete_extent_and_overlap) {
  const auto& form = Compiled<four>::get_representation();
  U32 values[] = {1, 2, 3, 4, 5};
  const ttx_storage input{&form, reinterpret_cast<U8*>(values), sizeof(values)};
  const ttx_storage output{&form, reinterpret_cast<U8*>(values + 1), 16};
  ASSERT_EQ(ttx_transfer(&input, &output), TTX_DATA_SUCCESS);

  const U32 expected[] = {1, 1, 2, 3, 4};
  for (Count index = 0; index != 5; ++index) {
    EXPECT_EQ(values[index], expected[index]);
  }

  EXPECT_EQ(ttx_transfer(&output, &output), TTX_DATA_SUCCESS);

  U32 destination[] = {90, 90, 90, 90, 90, 90};
  const ttx_storage spare{&form, reinterpret_cast<U8*>(destination + 1), 20};
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
  const auto input = Storage::from(value).get_abi();
  const auto target = Storage::from(output).get_abi();
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
  const auto different = Storage::from(real).get_abi();
  EXPECT_EQ(ttx_transfer(&different, &target), TTX_DATA_INCOMPATIBLE);

  const U8 malformed[] = {0xff};
  const ttx_representation malformed_form{malformed, sizeof(malformed)};
  const ttx_storage foreign{&malformed_form, input.data, input.size};
  EXPECT_EQ(ttx_transfer(&foreign, &target), TTX_DATA_INCOMPATIBLE);
  EXPECT_EQ(output, U32(99));
}

VALIDATION_TEST(Transfers, empty_value) {
  static constexpr auto empty =
      Schema::composite(Perimortem::Core::View::Vector<Schema::Position>(), 0);
  const auto& form = Compiled<empty>::get_representation();
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
      ++static_cast<State*>(context)->calls;
      return value + 1;
    }
  } state;

  DataOperation input{&state, State::call}, output{};
  EXPECT(
      Transfer::copy(Storage::from(input), Storage::from(output)) ==
      Status::Success);
  EXPECT_EQ(state.calls, U32(0));
  ASSERT(output.call);
  EXPECT_EQ(output.call(output.context, 41), U32(42));
  EXPECT_EQ(state.calls, U32(1));
}
