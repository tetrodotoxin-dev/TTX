// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/abi/receiver.hpp"
#include "validation/unit_tests/data/form/preparation.hpp"
#include "toolchain/validation/unit_test.hpp"
#include "ttx/data/protocol/direct/provider.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static Toolchain::Validation::Harness TtxDirect = {
  .name = "TTX::Data::Protocol::Direct",
};
static constexpr auto u32 = Schema::primitive(Schema::Value::U32);

// Data can use a Direct pointer without importing Semantic or any UUID type.
// The provider recovers its own state. The returned pointer is the explicitly
// public C representation, whose storage is retained by the fixture provider.
VALIDATION_TEST(TtxDirect, direct_without_bind) {
  Validation::DataTests::Preparation prepare;
  struct Source {
    const Representation& representation;
    U32 value;
  } source{
    prepare(u32),
    42,
  };
  const Ttx::Data::Protocol::Direct::Provider::Operations operations =
      Ttx::Data::Protocol::Direct::Provider::Operations(
          [](void* state) -> const Representation* {
            return &Ttx::Abi::Receiver::get<Source>(state).representation;
          },
          [](void* state) -> const void* {
            return &Ttx::Abi::Receiver::get<Source>(state).value;
          });

  Ttx::Data::Protocol::Direct::Provider access(&source, operations);
  EXPECT(access.get_representation().compatible(prepare(u32)));
  EXPECT_EQ(*static_cast<const U32*>(access.read_ptr()), U32(42));
}
