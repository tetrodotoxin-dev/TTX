// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/semantic/fixtures/portable_provider.h"
#include "toolchain/validation/unit_test.hpp"
#include "ttx/semantic/negotiation/query.hpp"

TTX_DATA_RECORD(
    portable_counter,
    TTX_DATA_MEMBER(portable_counter, receiver),
    TTX_DATA_MEMBER(portable_counter, add));

struct PortableCounter {
  using Api = portable_counter;
  static constexpr Perimortem::System::Uuid contract_id =
      Perimortem::System::Uuid(17, 23);
  Api api;
  explicit PortableCounter(Api api) : api(api) {}
};

struct WrongApi {
  const void* receiver;
  U64 (*add)(const void*, U64);
};

TTX_DATA_RECORD(
    WrongApi,
    TTX_DATA_MEMBER(WrongApi, receiver),
    TTX_DATA_MEMBER(WrongApi, add));

struct Wrong {
  using Api = WrongApi;
  static constexpr Perimortem::System::Uuid contract_id =
      Perimortem::System::Uuid(17, 23);
  explicit Wrong(Api) {}
};

static Toolchain::Validation::Harness PortableBinding = {
  .name = "TTX::PortableBinding",
};

// One UUID can describe the intended operation even when its C declaration
// drifted. Refuse the U64 declaration before calling anything, then negotiate
// the actual U32 API and invoke its opaque receiver through the returned thunk.
VALIDATION_TEST(PortableBinding, literal_c_descriptor) {
  using namespace Ttx::Semantic::Negotiation;
  const Query query(portable_counter_open());
  ASSERT(query.supports<PortableCounter>() == Binding::Status::Satisfied);

  const bool rejected = query.bind<Wrong>().visit(
      [](Wrong) { return false; },
      [](Binding::Failure failure) {
        return failure == Binding::Failure::Rejected;
      });
  ASSERT(rejected);
  ASSERT_EQ(portable_counter_calls(), U32(0));

  const bool called = query.bind<PortableCounter>().visit(
      [](PortableCounter counter) {
        return counter.api.add(counter.api.receiver, 2) == 42;
      },
      [](Binding::Failure) { return false; });
  EXPECT(called);
  EXPECT_EQ(portable_counter_calls(), U32(1));
}
