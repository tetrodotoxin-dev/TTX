// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/realization/simulacra.hpp"

#include <string.h>

#include "perimortem/core/static/vector.hpp"
#include "perimortem/core/null_terminated.hpp"

#include "validation/unit_tests/library.hpp"
#include "validation/unit_tests/semantic/fixtures/interface_provider.h"
#include "validation/unit_tests/semantic/measurement.hpp"
#include "toolchain/validation/unit_test.hpp"

using namespace Perimortem;
using namespace Ttx::Semantic::Negotiation;
using namespace Ttx::Semantic::Realization;
using Ttx::Data::Form::Compiled;
using Ttx::Data::Form::Native;
using Ttx::Data::Form::Storage;

TTX_DATA_RECORD(
    counter_api,
    TTX_DATA_MEMBER(counter_api, receiver),
    TTX_DATA_MEMBER(counter_api, add),
    TTX_DATA_MEMBER(counter_api, read));

static Toolchain::Validation::Harness TtxSimulacra = {
  .name = "TTX::Simulacra",
};

// This API is three words, with the functions themselves in the transferred
// record. Its facade owns those words while borrowing the foreign receiver.
class Counter {
 public:
  static constexpr auto contract_id =
      System::Uuid(COUNTER_ID_HIGH, COUNTER_ID_LOW);
  using Api = counter_api;
  static auto accept(Api api) -> Bool { return api.add && api.read; }

  explicit Counter(Api api) : api(api) {}
  auto add(U64 value) const -> U64 { return api.add(api.receiver, value); }
  auto read() const -> U64 { return api.read(api.receiver); }

 private:
  Api api;
};

class Foreign {
 public:
  Foreign()
      : module(Validation::open_library("libinterface_provider.so"_view)) {
    const auto open = reinterpret_cast<decltype(&interface_provider_open)>(
        Validation::find_symbol(module, "interface_provider_open"_view));
    api = open(&ttx_representation_compile);
  }

  counter_fixture api = counter_fixture();

 private:
  System::Library module;
};

// The semantic promise survives API drift. The first observation has no
// destination and cannot transfer a table. Binding still rejects a caller's
// incompatible format and leaves its bytes untouched.
VALIDATION_TEST(TtxSimulacra, support_without_abi) {
  Foreign foreign;
  const Query query(foreign.api.query);
  ASSERT(query.is_set());

  Validation::FlowTests::Measurement measurement;
  EXPECT(query.supports<Counter>() == Binding::Status::Satisfied);
  measurement.stop();

  EXPECT_EQ(measurement.get_allocations(), Count(0));
  if (const auto copies = measurement.get_copies()) {
    EXPECT_EQ(*copies, Count(0));
  }
  EXPECT_EQ(foreign.api.statistics().queries, U64(0));

  U64 output = 0x1234;
  const auto& form = Compiled<Native<U64>::reference>::get_representation();
  const Storage target(
      ttx_storage(&form, reinterpret_cast<U8*>(&output), sizeof(output)));
  EXPECT(query.bind(Counter::contract_id, target) == Binding::Status::Rejected);
  EXPECT_EQ(output, U64(0x1234));
  EXPECT(query.supports<Counter>() == Binding::Status::Satisfied);
  EXPECT_EQ(foreign.api.statistics().queries, U64(1));

  // The C entry answers the same question without any C++ descriptor machinery.
  EXPECT_EQ(
      foreign.api.query.supports(
          foreign.api.query.source, Counter::contract_id),
      TTX_BINDING_SATISFIED);
}

// A refused or unsettled property cannot become false just because the caller
// wanted a predicate. Retaining statuses keeps a later policy from bypassing
// this answer or treating provisional absence as a completed fact.
VALIDATION_TEST(TtxSimulacra, support_outcomes) {
  Foreign foreign;
  const Query query(foreign.api.query);
  ASSERT(query.is_set());

  const Perimortem::Core::Static::Vector<Binding::Status, 3> statuses = {
    {
      Binding::Status::Satisfied,
      Binding::Status::Unknown,
      Binding::Status::Rejected,
    },
  };
  for (const auto status : statuses.get_view()) {
    foreign.api.reset(static_cast<ttx_binding_status>(status), 0, 0);
    EXPECT(query.supports<Counter>() == status);
    EXPECT_EQ(foreign.api.statistics().queries, U64(0));
  }

  const U8 invalid_statuses[] = {2, 99};
  for (const auto invalid : invalid_statuses) {
    foreign.api.reset(invalid, 0, 0);
    EXPECT(query.supports<Counter>() == Binding::Status::Rejected);
  }
  EXPECT(Query().supports<Counter>() == Binding::Status::Rejected);
}

// The provider compiles its own C Schema at module opening. C++ derives this
// side's description from counter_api. One checked bind copies the entire API.
// All later calls use the acquired functions without metadata or allocation.
VALIDATION_TEST(TtxSimulacra, retained_c_calls) {
  Foreign foreign;
  ASSERT(foreign.api.query.bind);
  Simulacra::fulfill<Counter>(Query(foreign.api.query))
      .visit(
          [&](const Counter& counter) {
            Validation::FlowTests::Measurement measurement;
            U64 answer = 0;
            for (U64 i = 0; i < 2; ++i) {
              answer = counter.add(1);
            }
            measurement.stop();

            EXPECT_EQ(answer, U64(2));
            EXPECT_EQ(counter.read(), answer);
            EXPECT_EQ(measurement.get_allocations(), Count(0));
            if (const auto copies = measurement.get_copies()) {
              EXPECT_EQ(*copies, Count(0));
            }
          },
          [&](Binding::Failure) { EXPECT(False); });

  EXPECT_EQ(foreign.api.statistics().queries, U64(1));
  EXPECT_EQ(foreign.api.statistics().calls, U64(3));
}

VALIDATION_TEST(TtxSimulacra, refusal_boundaries) {
  Foreign foreign;
  ASSERT(foreign.api.query.bind);
  const Perimortem::Core::Static::Vector<ttx_binding_status, 4> statuses = {
    {
      TTX_BINDING_UNKNOWN,
      TTX_BINDING_REJECTED,
      2,
      99,
    },
  };
  for (const auto status : statuses.get_view()) {
    foreign.api.reset(status, 0, 0);
    Query(foreign.api.query)
        .bind<Counter>()
        .visit(
            [&](const Counter&) { EXPECT(False); },
            [&](Binding::Failure error) {
              EXPECT_EQ(
                  static_cast<U8>(error),
                  status == 2 || status == 99 ? TTX_BINDING_REJECTED : status);
            });
    EXPECT_EQ(foreign.api.statistics().queries, U64(1));
    EXPECT_EQ(foreign.api.statistics().calls, U64(0));
  }

  // The generic exchange accepts values according to their contract. Counter
  // requires both functions, so a provider that leaves the output empty cannot
  // produce its typed view. A null opaque receiver remains valid.
  foreign.api.reset(TTX_BINDING_SATISFIED, 1, 0);
  Query(foreign.api.query)
      .bind<Counter>()
      .visit(
          [&](const Counter&) { EXPECT(False); },
          [&](Binding::Failure error) {
            EXPECT(error == Binding::Failure::Rejected);
          });
  foreign.api.reset(TTX_BINDING_SATISFIED, 0, 1);
  Query(foreign.api.query)
      .bind<Counter>()
      .visit(
          [&](const Counter& counter) { EXPECT_EQ(counter.add(17), U64(17)); },
          [&](Binding::Failure) { EXPECT(False); });
}

struct WrongResult {
  void* receiver;
  R64 (*add)(void*, U64);
  U64 (*read)(void*);
};
struct WrongArguments {
  void* receiver;
  U64 (*add)(U64);
  U64 (*read)(void*);
};
struct WrongAbi {
  void* receiver;
  U64 (*add)(void*, U64, ...);
  U64 (*read)(void*);
};
struct WrongOrder {
  void* receiver;
  U64 (*read)(void*);
  U64 (*add)(void*, U64);
};
TTX_DATA_RECORD(
    WrongResult,
    TTX_DATA_MEMBER(WrongResult, receiver),
    TTX_DATA_MEMBER(WrongResult, add),
    TTX_DATA_MEMBER(WrongResult, read));
TTX_DATA_RECORD(
    WrongArguments,
    TTX_DATA_MEMBER(WrongArguments, receiver),
    TTX_DATA_MEMBER(WrongArguments, add),
    TTX_DATA_MEMBER(WrongArguments, read));
TTX_DATA_RECORD(
    WrongAbi,
    TTX_DATA_MEMBER(WrongAbi, receiver),
    TTX_DATA_MEMBER(WrongAbi, add),
    TTX_DATA_MEMBER(WrongAbi, read));
TTX_DATA_RECORD(
    WrongOrder,
    TTX_DATA_MEMBER(WrongOrder, receiver),
    TTX_DATA_MEMBER(WrongOrder, read),
    TTX_DATA_MEMBER(WrongOrder, add));

// Every record has the same byte extent and requests the same semantic UUID.
// Only the callable descriptors differ. Rejection must precede copying or
// invocation, including when the disagreement is solely the calling ABI.
VALIDATION_TEST(TtxSimulacra, mismatched_callables) {
  Foreign foreign;
  ASSERT(foreign.api.query.bind);
  Perimortem::Core::Static::Vector<const Ttx::Data::Form::Representation*, 4>
      forms = {
        {
          &Compiled<Native<WrongResult>::reference>::get_representation(),
          &Compiled<Native<WrongArguments>::reference>::get_representation(),
          &Compiled<Native<WrongAbi>::reference>::get_representation(),
          &Compiled<Native<WrongOrder>::reference>::get_representation(),
        },
      };
  alignas(counter_api) Perimortem::Core::Static::Vector<U8, sizeof(counter_api)>
      output;
  Perimortem::Core::Static::Vector<U8, sizeof(output)> expected;
  memset(expected.get_data(), 0xa5, sizeof(expected));
  for (const auto* form : forms.get_view()) {
    memcpy(output.get_data(), expected.get_data(), sizeof(output));
    const auto status =
        Query(foreign.api.query)
            .bind(
                Counter::contract_id,
                Storage(ttx_storage(form, output.get_data(), sizeof(output))));
    EXPECT(status == Binding::Status::Rejected);
    EXPECT(memcmp(output.get_data(), expected.get_data(), sizeof(output)) == 0);
  }

  EXPECT_EQ(foreign.api.statistics().calls, U64(0));
}

class Local {
 public:
  Local(Query fallback, Binding::Status status)
      : fallback(fallback), status(status) {}
  template <typename Contract>
  auto fulfill_native() const -> Utility::Result<Contract, Binding::Failure> {
    if (status != Binding::Status::Satisfied) {
      return static_cast<Binding::Failure>(status);
    }
    if constexpr (__is_same(Contract, Counter)) {
      const counter_api api = counter_api(
          nullptr, [](void*, U64 amount) -> U64 { return amount; },
          [](void*) -> U64 { return 0; });
      return Counter(api);
    }
    return Binding::Failure::Unknown;
  }
  auto get_query() const -> Query { return fallback; }

 private:
  Query fallback;
  Binding::Status status;
};

VALIDATION_TEST(TtxSimulacra, native_policy) {
  Foreign foreign;
  ASSERT(foreign.api.query.bind);
  const Perimortem::Core::Static::Vector<Binding::Status, 3> statuses = {
    {
      Binding::Status::Satisfied,
      Binding::Status::Unknown,
      Binding::Status::Rejected,
    },
  };
  for (auto status : statuses.get_view()) {
    foreign.api.reset(TTX_BINDING_SATISFIED, 0, 0);
    Local native(Query(foreign.api.query), status);
    Simulacra::fulfill<Counter>(native).visit(
        [&](const Counter& counter) {
          EXPECT(
              status == Binding::Status::Satisfied ||
              status == Binding::Status::Unknown);
          EXPECT_EQ(counter.add(9), U64(9));
        },
        [&](Binding::Failure failure) {
          EXPECT_EQ(static_cast<U8>(failure), static_cast<U8>(status));
        });
    EXPECT_EQ(
        foreign.api.statistics().queries,
        status == Binding::Status::Unknown ? U64(1) : U64(0));
  }
}
