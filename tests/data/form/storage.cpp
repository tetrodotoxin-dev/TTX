// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/storage.hpp"

#include "tests/data/form/preparation.hpp"
#include "toolchain/validation/unit_test.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static Toolchain::Validation::Harness TtxStorage = {
  .name = "TTX::Data::Form::Storage",
};
static constexpr auto u32 = Schema::primitive(Schema::Value::U32);

VALIDATION_TEST(TtxStorage, storage_check) {
  Validation::DataTests::Preparation prepare;
  const auto& representation = prepare(u32);
  U32 value = 0;
  EXPECT(
      Storage::create(
          representation, {reinterpret_cast<U8*>(&value), sizeof(value)})
          .visit(
              [](Storage storage) {
                return storage.get_bytes().get_size() == sizeof(U32);
              },
              [](Status) { return false; }));

  Storage::create(
      representation,
      {
        reinterpret_cast<U8*>(&value),
        2,
      })
      .visit(
          [&](Storage) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Bounds); });
}
