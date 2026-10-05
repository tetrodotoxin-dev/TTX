// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

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
// Its functions interpret their own context and return the public C
// representation retained by that provider.
VALIDATION_TEST(TtxDirect, direct_without_bind) {
  Validation::DataTests::Preparation prepare;
  struct Source {
    const Representation& representation;
    U32 value;

    static auto describe(void* context) -> const Representation* {
      return &static_cast<Source*>(context)->representation;
    }

    static auto read(void* context) -> const void* {
      return &static_cast<Source*>(context)->value;
    }
  } source{
    prepare(u32),
    42,
  };
  const Ttx::Data::Protocol::Direct::Provider::Operations operations = {
    Source::describe, Source::read};

  Ttx::Data::Protocol::Direct::Provider access(&source, operations);
  EXPECT(access.get_representation().compatible(prepare(u32)));
  EXPECT_EQ(*static_cast<const U32*>(access.read_ptr()), U32(42));
}
