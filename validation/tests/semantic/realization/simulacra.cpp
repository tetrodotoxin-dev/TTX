// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include <string.h>

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/storage/vector.hpp"
#include "perimortem/core/view/null_terminated.hpp"
#include "perimortem/memory/allocator/bibliotheca.hpp"

#include "ttx/semantic/negotiation/query.hpp"
#include "validation/providers/interface/provider.h"
#include "validation/support/library.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Perimortem::Serialization;
using namespace Perimortem::System;
using namespace Toolchain::Validation;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

TTX_DATA_RECORD(
    counter_api,
    TTX_DATA_MEMBER(counter_api, context),
    TTX_DATA_MEMBER(counter_api, add),
    TTX_DATA_MEMBER(counter_api, read));

static Harness TtxSimulacra = {
  .name = "TTX::Simulacra",
};

// This API is three words, with the functions themselves in the transferred
// record. The C++ interface stores those words and uses the context supplied
// with them for each call.
class Counter {
 public:
  static constexpr auto contract_id =
      Formats::Uuid(COUNTER_ID_HIGH, COUNTER_ID_LOW);
  using Api = counter_api;

  static auto accept(Api api) -> Bool { return api.add && api.read; }

  // Copies a negotiated API record. Explicit construction marks the unchecked
  // conversion from that C record. The supplying contract keeps its context
  // and functions available.
  explicit Counter(Api api) : api(api) {}

  auto get_abi() const -> Api { return api; }

  auto add(U64 value) const -> U64 { return api.add(api.context, value); }

  auto read() const -> U64 { return api.read(api.context); }

 private:
  Api api;
};

class Foreign {
 public:
  Foreign()
      : module(
            Validation::Support::open_library(
                "libinterface_provider.so"_view)) {
    const auto open = Data::cast<decltype(interface_provider_open)>(
        Validation::Support::find_symbol(
            module, "interface_provider_open"_view));
    api = open(&ttx_representation_compile);
  }

  counter_fixture api = counter_fixture();

 private:
  Process::Library module;
};

// The semantic promise survives API drift. The first observation has no
// destination and cannot transfer a table. Binding still rejects a caller's
// incompatible format and leaves its bytes untouched.
VALIDATION_TEST(TtxSimulacra, support_without_abi) {
  Foreign foreign;
  const Negotiation::Query query(foreign.api.query);
  ASSERT(query.is_set());

  const Count requests = Allocator::Bibliotheca::check_out_requests();
  EXPECT(query.supports<Counter>() == Negotiation::Status::Satisfied);
  EXPECT_EQ(Allocator::Bibliotheca::check_out_requests(), requests);

  EXPECT_EQ(foreign.api.statistics().queries, U64(0));

  U64 output = 0x1234;
  const auto& form =
      Form::Compiled<Form::Native<U64>::reference>::get_representation();
  const Form::Storage target(
      ttx_storage(&form.get_abi(), Data::cast<U8>(&output), sizeof(output)));
  EXPECT(
      query.bind(Counter::contract_id, target) ==
      Negotiation::Status::Rejected);
  EXPECT_EQ(output, U64(0x1234));
  EXPECT(query.supports<Counter>() == Negotiation::Status::Satisfied);
  EXPECT_EQ(foreign.api.statistics().queries, U64(1));

  // The C operation reports the same support status directly from its UUID.
  EXPECT_EQ(
      foreign.api.query.supports(
          foreign.api.query.context, Counter::contract_id),
      TTX_NEGOTIATION_SATISFIED);
}

// An identical API representation does not establish the requested meaning.
// UUID selection and representation agreement are independent conditions.
VALIDATION_TEST(TtxSimulacra, different_meaning_same_representation) {
  Foreign foreign;
  counter_api api = {};
  const Formats::Uuid other(COUNTER_ID_HIGH, COUNTER_ID_LOW + 1);
  const Negotiation::Query query(foreign.api.query);
  EXPECT(query.supports(other) == Negotiation::Status::Unknown);
  EXPECT(
      query.bind(
          other,
          Form::Storage(ttx_storage(
              &Form::Compiled<
                   Form::Native<Counter::Api>::reference>::get_representation()
                   .get_abi(),
              Data::cast<U8>(&api), sizeof(api)))) ==
      Negotiation::Status::Unknown);
  EXPECT_EQ(foreign.api.statistics().calls, U64(0));
}

// Support queries preserve all three negotiation outcomes. An undetermined
// contract remains distinguishable from an explicitly refused contract.
VALIDATION_TEST(TtxSimulacra, support_outcomes) {
  Foreign foreign;
  const Negotiation::Query query(foreign.api.query);
  ASSERT(query.is_set());

  const Storage::Vector<Negotiation::Status, 3> statuses = {
    {
      Negotiation::Status::Satisfied,
      Negotiation::Status::Unknown,
      Negotiation::Status::Rejected,
    },
  };

  for (Count status_index = 0; status_index < statuses.get_size();
       ++status_index) {
    const auto status = statuses[status_index];
    foreign.api.reset(ttx_negotiation_status(status), 0, 0);
    EXPECT(query.supports<Counter>() == status);
    EXPECT_EQ(foreign.api.statistics().queries, U64(0));
  }

  const U8 invalid_statuses[] = {2, 99};
  for (const auto invalid : invalid_statuses) {
    foreign.api.reset(invalid, 0, 0);
    EXPECT(query.supports<Counter>() == Negotiation::Status::Rejected);
  }

  EXPECT(
      Negotiation::Query().supports<Counter>() ==
      Negotiation::Status::Rejected);
}

// The provider compiles its own C Schema at module opening. C++ derives this
// side's description from counter_api. One checked bind copies the entire API.
// All later calls use the acquired functions without metadata or allocation.
VALIDATION_TEST(TtxSimulacra, retained_c_calls) {
  Foreign foreign;
  ASSERT(foreign.api.query.bind);
  Negotiation::Query(foreign.api.query)
      .bind<Counter>()
      .visit(
          [&](const Counter& counter) {
            const Count requests = Allocator::Bibliotheca::check_out_requests();
            U64 answer = 0;
            for (U64 i = 0; i < 2; ++i) {
              answer = counter.add(1);
            }

            EXPECT_EQ(answer, U64(2));
            EXPECT_EQ(Allocator::Bibliotheca::check_out_requests(), requests);
            EXPECT_EQ(counter.read(), answer);
          },

          [&](Negotiation::Status) { EXPECT(False); });

  EXPECT_EQ(foreign.api.statistics().queries, U64(1));
  EXPECT_EQ(foreign.api.statistics().calls, U64(3));
}

VALIDATION_TEST(TtxSimulacra, refusal_boundaries) {
  Foreign foreign;
  ASSERT(foreign.api.query.bind);

  const Storage::Vector<ttx_negotiation_status, 4> statuses = {
    {
      TTX_NEGOTIATION_UNKNOWN,
      TTX_NEGOTIATION_REJECTED,
      2,
      99,
    },
  };

  for (Count status_index = 0; status_index < statuses.get_size();
       ++status_index) {
    const auto status = statuses[status_index];
    foreign.api.reset(status, 0, 0);
    Negotiation::Query(foreign.api.query)
        .bind<Counter>()
        .visit(
            [&](const Counter&) { EXPECT(False); },
            [&](Negotiation::Status error) {
              EXPECT_EQ(
                  U8(error), status == 2 || status == 99
                                 ? TTX_NEGOTIATION_REJECTED
                                 : status);
            });

    EXPECT_EQ(foreign.api.statistics().queries, U64(1));
    EXPECT_EQ(foreign.api.statistics().calls, U64(0));
  }

  // The generic exchange accepts values according to their contract. Counter
  // requires both functions, so a provider that leaves the output empty cannot
  // produce its typed view. A null opaque context remains valid.
  foreign.api.reset(TTX_NEGOTIATION_SATISFIED, 1, 0);
  Negotiation::Query(foreign.api.query)
      .bind<Counter>()
      .visit(
          [&](const Counter&) { EXPECT(False); },
          [&](Negotiation::Status error) {
            EXPECT(error == Negotiation::Status::Rejected);
          });

  foreign.api.reset(TTX_NEGOTIATION_SATISFIED, 0, 1);
  Negotiation::Query(foreign.api.query)
      .bind<Counter>()
      .visit(
          [&](const Counter& counter) { EXPECT_EQ(counter.add(17), U64(17)); },
          [&](Negotiation::Status) { EXPECT(False); });
}

struct WrongResult {
  void* context;
  R64 (*add)(void*, U64);
  U64 (*read)(void*);
};

struct WrongArguments {
  void* context;
  U64 (*add)(U64);
  U64 (*read)(void*);
};

struct WrongAbi {
  void* context;
  U64 (*add)(void*, U64, ...);
  U64 (*read)(void*);
};

struct WrongOrder {
  void* context;
  U64 (*read)(void*);
  U64 (*add)(void*, U64);
};

TTX_DATA_RECORD(
    WrongResult,
    TTX_DATA_MEMBER(WrongResult, context),
    TTX_DATA_MEMBER(WrongResult, add),
    TTX_DATA_MEMBER(WrongResult, read));
TTX_DATA_RECORD(
    WrongArguments,
    TTX_DATA_MEMBER(WrongArguments, context),
    TTX_DATA_MEMBER(WrongArguments, add),
    TTX_DATA_MEMBER(WrongArguments, read));
TTX_DATA_RECORD(
    WrongAbi,
    TTX_DATA_MEMBER(WrongAbi, context),
    TTX_DATA_MEMBER(WrongAbi, add),
    TTX_DATA_MEMBER(WrongAbi, read));
TTX_DATA_RECORD(
    WrongOrder,
    TTX_DATA_MEMBER(WrongOrder, context),
    TTX_DATA_MEMBER(WrongOrder, read),
    TTX_DATA_MEMBER(WrongOrder, add));

// Every record has the same byte extent and requests the same semantic UUID.
// Only the callable descriptors differ. Rejection must precede copying or
// invocation, including when the disagreement is solely the calling ABI.
VALIDATION_TEST(TtxSimulacra, mismatched_callables) {
  Foreign foreign;
  ASSERT(foreign.api.query.bind);

  Storage::Vector<const Form::Representation*, 4> forms = {
    {
      &Form::Compiled<
          Form::Native<WrongResult>::reference>::get_representation(),
      &Form::Compiled<
          Form::Native<WrongArguments>::reference>::get_representation(),
      &Form::Compiled<Form::Native<WrongAbi>::reference>::get_representation(),
      &Form::Compiled<
          Form::Native<WrongOrder>::reference>::get_representation(),
    },
  };

  alignas(counter_api) Storage::Vector<U8, sizeof(counter_api)> output;

  Storage::Vector<U8, sizeof(output)> expected;
  memset(expected.get_data(), 0xa5, sizeof(expected));
  for (Count form_index = 0; form_index < forms.get_size(); ++form_index) {
    const auto* form = forms[form_index];
    memcpy(output.get_data(), expected.get_data(), sizeof(output));

    const auto status =
        Negotiation::Query(foreign.api.query)
            .bind(
                Counter::contract_id,
                Form::Storage(ttx_storage(
                    &form->get_abi(), output.get_data(), sizeof(output))));
    EXPECT(status == Negotiation::Status::Rejected);
    EXPECT(memcmp(output.get_data(), expected.get_data(), sizeof(output)) == 0);
  }

  EXPECT_EQ(foreign.api.statistics().calls, U64(0));
}

// Counter operations can use a U64 directly, find it inside a private C struct,
// or construct a temporary value for each call. Every API record supplies the
// context its functions expect. A later bind may select different functions
// and context while the earlier records remain callable.
VALIDATION_TEST(TtxSimulacra, composed_contexts) {
  Foreign foreign;
  const Negotiation::Query query(foreign.api.query);
  Counter direct({}), independent({}), embedded({}), temporary({});
  const auto acquire = [&](counter_composition mode, Counter& output) {
    foreign.api.compose(mode);
    query.bind<Counter>().visit(
        [&](Counter counter) { output = counter; },
        [&](Negotiation::Status) { EXPECT(False); });
  };

  acquire(COUNTER_DIRECT, direct);
  acquire(COUNTER_SECOND, independent);
  acquire(COUNTER_EMBEDDED, embedded);
  acquire(COUNTER_TEMPORARY, temporary);
  ASSERT(
      Counter::accept(direct.get_abi()) &&
      Counter::accept(independent.get_abi()) &&
      Counter::accept(embedded.get_abi()) &&
      Counter::accept(temporary.get_abi()));
  EXPECT(direct.get_abi().read == independent.get_abi().read);
  EXPECT(direct.get_abi().context != independent.get_abi().context);
  EXPECT_EQ(counter_consume(independent.get_abi(), 2), U64(22));
  EXPECT(direct.get_abi().context != embedded.get_abi().context);
  EXPECT(embedded.get_abi().context == temporary.get_abi().context);
  EXPECT(embedded.get_abi().read != temporary.get_abi().read);
  EXPECT_EQ(direct.add(3), U64(3));
  EXPECT_EQ(embedded.read(), U64(3));
  EXPECT_EQ(temporary.read(), U64(103));
  EXPECT_EQ(counter_consume(temporary.get_abi(), 4), U64(107));
  EXPECT_EQ(counter_consume(embedded.get_abi(), 5), U64(12));
  EXPECT_EQ(direct.read(), U64(12));

  // Copies retain the supplied functions and context, regardless of the next
  // selection. Query's context never determines the returned interface type.
  Counter first({}), second({});
  foreign.api.compose(COUNTER_ALTERNATING);
  query.bind<Counter>().visit(
      [&](Counter value) { first = value; },
      [&](Negotiation::Status) { EXPECT(False); });
  query.bind<Counter>().visit(
      [&](Counter value) { second = value; },
      [&](Negotiation::Status) { EXPECT(False); });
  ASSERT(Counter::accept(first.get_abi()) && Counter::accept(second.get_abi()));
  EXPECT(first.get_abi().context != second.get_abi().context);
  EXPECT_EQ(first.read(), U64(12));
  EXPECT_EQ(second.read(), U64(112));
}
