// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "ttx/semantic/negotiation/query.h"
#include "validation/tests/semantic/fixtures/portable_provider.h"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Semantic;

TTX_DATA_RECORD(
    portable_counter,
    TTX_DATA_MEMBER(portable_counter, context),
    TTX_DATA_MEMBER(portable_counter, add));

struct PortableCounter {
  using Api = portable_counter;
  static constexpr Identity::Uuid contract_id = Identity::Uuid(17, 23);
  Api api;

  // Copies a negotiated record whose provider supplies callable code and its
  // required context. Explicit construction marks this unchecked conversion
  // from the C record.
  explicit PortableCounter(Api api) : api(api) {}
};

struct WrongApi {
  const void* context;
  U64 (*add)(const void*, U64);
};

TTX_DATA_RECORD(
    WrongApi,
    TTX_DATA_MEMBER(WrongApi, context),
    TTX_DATA_MEMBER(WrongApi, add));

struct Wrong {
  using Api = WrongApi;
  static constexpr Identity::Uuid contract_id = Identity::Uuid(17, 23);
  // Explicit construction marks the conversion that a successful bind would
  // select. This fixture requests an incompatible signature and expects that
  // bind to reject the record before reaching this constructor.
  explicit Wrong(Api) {}
};

static Harness PortableBinding = {
  .name = "TTX::PortableBinding",
};

// A C consumer calls a C++ support operation through the published Query
// record. Its return byte and Data's callable descriptor must agree with
// the original unsigned-byte ABI for all three negotiation outcomes.
VALIDATION_TEST(PortableBinding, scoped_status_from_cpp) {
  using Status = Negotiation::Status;
  using namespace Ttx::Data;
  using ByteSupport = U8 (*)(void*, perimortem_uuid);
  using TypedSupport = decltype(ttx_semantic_query::supports);
  static_assert(__is_same(ttx_negotiation_status, Status));
  static_assert(__is_same(__underlying_type(Status), U8));
  static_assert(__is_same(TypedSupport, Status (*)(void*, perimortem_uuid)));
  static_assert(
      Form::Compiled<
          Form::Native<TypedSupport>::reference>::get_representation()
          .compatible(
              Form::Compiled<
                  Form::Native<ByteSupport>::reference>::get_representation()));

  Status decision = Status::Satisfied;
  const ttx_semantic_query query{
    &decision, nullptr, [](void* context, perimortem_uuid) -> Status {
      return *Data::cast<Status>(context);
    }};

  EXPECT_EQ(portable_support_code(query), U8(0));
  decision = Status::Unknown;
  EXPECT_EQ(portable_support_code(query), U8(1));
  decision = Status::Rejected;
  EXPECT_EQ(portable_support_code(query), U8(3));
}

// One UUID can describe the intended operation even when its C declaration
// drifted. Refuse the U64 declaration before calling anything, then negotiate
// the actual U32 API and call the supplied function with its opaque context.
VALIDATION_TEST(PortableBinding, literal_c_descriptor) {
  const Negotiation::Query query(portable_counter_open());
  ASSERT(query.supports<PortableCounter>() == Negotiation::Status::Satisfied);

  const bool rejected = query.bind<Wrong>().visit(
      [](Wrong) { return false; },
      [](Negotiation::Status failure) {
        return failure == Negotiation::Status::Rejected;
      });

  ASSERT(rejected);
  ASSERT_EQ(portable_counter_calls(), U32(0));

  const bool called = query.bind<PortableCounter>().visit(
      [](PortableCounter counter) {
        return counter.api.add(counter.api.context, 2) == 42;
      },

      [](Negotiation::Status) { return false; });
  EXPECT(called);
  EXPECT_EQ(portable_counter_calls(), U32(1));
}
