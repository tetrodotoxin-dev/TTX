// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/realization/invocation.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/storage/vector.hpp"
#include "perimortem/core/view/null_terminated.hpp"
#include "perimortem/memory/allocator/bibliotheca.hpp"

#include "ttx/data/form/compiled.hpp"
#include "validation/providers/invocation/provider.h"
#include "validation/support/library.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Perimortem::Serialization;
using namespace Perimortem::System;
using namespace Toolchain::Validation;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness InvocationTests = {
  .name = "TTX::Invocation",
};

static constexpr auto integer =
    Form::Schema::primitive(Form::Schema::Value::S64);
static constexpr auto real = Form::Schema::primitive(Form::Schema::Value::R64);
static constexpr auto flag = Form::Schema::primitive(Form::Schema::Value::U8);
static constexpr Storage::Vector<Form::Schema::Position, 3> positions = {
  {
    Form::Schema::Position(
        integer,
        __builtin_offsetof(invocation_input, value)),
    Form::Schema::Position(real, __builtin_offsetof(invocation_input, scale)),
    Form::Schema::Position(flag, __builtin_offsetof(invocation_input, negate)),
  },
};

static constexpr auto inputs = Form::Schema::composite(
    positions,
    sizeof(invocation_input),
    alignof(invocation_input));
static constexpr Formats::Uuid operation =
    Formats::Uuid(INVOCATION_METHOD_HIGH, INVOCATION_METHOD_LOW);

class ForeignInvocation {
 public:
  ForeignInvocation()
      : module(
            Validation::Support::open_library(
                "libinvocation_provider.so"_view)) {
    const auto open = Data::cast<decltype(invocation_provider_open)>(
        Validation::Support::find_symbol(
            module, "invocation_provider_open"_view));
    api = open(ttx_invocation_representation(), &input_form(), &output_form());
  }

  static auto input_form() -> const Form::Representation& {
    return Form::Compiled<inputs>::get_representation();
  }

  static auto output_form() -> const Form::Representation& {
    return Form::Compiled<real>::get_representation();
  }

  invocation_fixture api = {};

 private:
  Process::Library module;
};

// The consumer negotiates once, checks the buffer layouts and calls the same
// operation sixteen times while the provider library remains loaded. The
// bind, transfer and allocation counters must stay unchanged across the calls.
VALIDATION_TEST(InvocationTests, generated_results) {
  ForeignInvocation foreign;
  ASSERT(foreign.api.query.bind);
  Negotiation::Query(foreign.api.query)
      .bind<Realization::Invocation>(operation)
      .visit(
          [&](Realization::Invocation call) {
            ASSERT(call.accepts(
                ForeignInvocation::input_form(),
                ForeignInvocation::output_form()));
            EXPECT(call.get_abi().context != foreign.api.query.context);
            EXPECT_NOT(call.accepts(
                ForeignInvocation::input_form(),
                Form::Compiled<integer>::get_representation()));

            const auto established = foreign.api.statistics();
            const Count requests = Allocator::Bibliotheca::check_out_requests();
            for (S64 index = 0; index != 16; ++index) {
              const invocation_input arguments = {index, 1.5, U8(index % 2)};
              R64 output = 0;
              ASSERT(call.invoke(&arguments, &output) == Status::Success);

              const R64 expected = (index + 2) * 1.5;
              EXPECT_EQ(output, index % 2 ? -expected : expected);
            }

            EXPECT_EQ(Allocator::Bibliotheca::check_out_requests(), requests);
            EXPECT_EQ(foreign.api.statistics().binds, established.binds);
            EXPECT_EQ(
                foreign.api.statistics().transfers, established.transfers);
            EXPECT_EQ(foreign.api.statistics().calls, U64(16));
          },

          [&](Negotiation::Status) { EXPECT(False); });
}

VALIDATION_TEST(InvocationTests, refused_and_malformed_records) {
  ForeignInvocation foreign;
  const ttx_negotiation_status failures[] = {
    TTX_NEGOTIATION_UNKNOWN, TTX_NEGOTIATION_REJECTED};

  for (const auto failure : failures) {
    foreign.api.configure(failure, 0);
    Negotiation::Query(foreign.api.query)
        .bind<Realization::Invocation>(operation)
        .visit(
            [&](Realization::Invocation) { EXPECT(False); },
            [&](Negotiation::Status status) {
              EXPECT_EQ(ttx_negotiation_status(status), failure);
            });

    EXPECT_EQ(foreign.api.statistics().transfers, U64(0));
    EXPECT_EQ(foreign.api.statistics().calls, U64(0));
  }

  foreign.api.configure(TTX_NEGOTIATION_SATISFIED, 1);
  Negotiation::Query(foreign.api.query)
      .bind<Realization::Invocation>(operation)
      .visit(
          [&](Realization::Invocation) { EXPECT(False); },
          [&](Negotiation::Status status) {
            EXPECT(status == Negotiation::Status::Rejected);
          });

  EXPECT_EQ(foreign.api.statistics().transfers, U64(1));
  EXPECT_EQ(foreign.api.statistics().calls, U64(0));
}
