// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/storage.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"

#include "validation/tests/data/form/preparation.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Data;

static Harness TtxStorage = {
  .name = "TTX::Data::Form::Storage",
};

static constexpr auto u32 = Form::Schema::primitive(Form::Schema::Value::U32);

VALIDATION_TEST(TtxStorage, storage_check) {
  Validation::DataTests::Preparation prepare;
  const auto& representation = prepare(u32);
  U32 value = 0;
  EXPECT(
      Form::Storage::create(
          representation, {*Data::cast<U8>(&value), sizeof(value)})
          .visit(
              [](Form::Storage storage) {
                return storage.get_bytes().get_size() == sizeof(U32);
              },

              [](Status) { return false; }));

  Form::Storage::create(
      representation,
      {
        *Data::cast<U8>(&value),
        2,
      })

      .visit(
          [&](Form::Storage) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Bounds); });
}

// Empty represented values retain their form while observation uses the
// canonical empty Perimortem View. Writable Access still refers to storage.
VALIDATION_TEST(TtxStorage, empty_observation) {
  static constexpr auto empty =
      Form::Schema::composite(View::Vector<Form::Schema::Position>(), 0);
  const auto& form = Form::Compiled<empty>::get_representation();
  const Form::Storage absent({&form, nullptr, 0});
  EXPECT(absent.get_bytes().is_empty());
  EXPECT(absent.get_bytes().get_data() == nullptr);

  U8 backing = 0;
  Form::Storage::create(form, Access::Bytes(backing, 0))
      .visit(
          [&](Form::Storage storage) {
            EXPECT(storage.get_bytes().is_empty());
            EXPECT(storage.get_abi().data == &backing);
          },

          [&](Status) { EXPECT(False); });
}
