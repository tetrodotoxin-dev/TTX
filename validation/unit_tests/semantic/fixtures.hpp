// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#pragma once

#include "validation/unit_tests/data/form/preparation.hpp"
#include "validation/unit_tests/semantic/module.hpp"

#include <stdio.h>
#include <stdlib.h>

#include "perimortem/core/access/vector.hpp"
#include "perimortem/core/static/vector.hpp"
#include "perimortem/core/null_terminated.hpp"

#include "toolchain/validation/unit_test.hpp"
#include "ttx/data/form/schema.hpp"
#include "ttx/semantic/flows/copy.hpp"
#include "ttx/semantic/flows/swizzle.hpp"
#include "ttx/semantic/transport/flow.hpp"
#include "validation/providers/flow/provider.h"
#include "validation/providers/heterogeneous/provider.h"

namespace Validation::FlowTests {
using namespace Perimortem::Core;
using Ttx::Data::Status;
using Ttx::Data::Form::Representation;
using Ttx::Data::Form::Schema;
using Ttx::Data::Form::Storage;
using namespace Ttx::Semantic::Negotiation;
using Ttx::Semantic::Flows::Copy;
using Ttx::Semantic::Flows::Swizzle;
using Ttx::Semantic::Negotiation::Query;
using Ttx::Semantic::Transport::Flow;
using Protocol = Flow::Protocol;
extern Toolchain::Validation::Harness TtxFlow;
inline constexpr auto integer = Schema::primitive(Schema::Value::U32);
inline constexpr auto real = Schema::primitive(Schema::Value::R32);
using Validation::DataTests::Preparation;
inline constexpr auto four_schema = Schema::range(integer, 4, 4, 16, 4);

// This consumer is just a protocol policy and an ABI requirement. It owns no
// destination. A Storage may supply such a query too, without changing Flow.
struct Consumer {
  const Representation& representation;
  U8 provides =
      PROVIDES_DIRECT | PROVIDES_SHARED | PROVIDES_BLOCK | PROVIDES_FRAGMENT;
  Perimortem::Core::Static::Vector<Count, 4> binds = {};
  Count descriptions = 0;
  Binding::Status decline = Binding::Status::Unknown;
  const Representation* direct_representation = nullptr;

  auto query() -> Query;

  static auto describe(void*) -> const Representation*;

  static auto describe_direct(void*) -> const Representation*;

  static auto bind(void*, perimortem_uuid, ttx_storage) -> ttx_binding_status;

  static auto supports(void*, perimortem_uuid) -> ttx_binding_status;
};

template <typename T>
auto storage(const Representation& representation, T& data) -> Storage {
  return Storage::create(
             representation,
             {
               reinterpret_cast<U8*>(&data),
               sizeof(data),
             })
      .visit(
          [](Storage value) { return value; },
          [](Status) -> Storage {
            fputs("Invalid fixture Storage.", stderr);
            abort();
          });
}

}  // namespace Validation::FlowTests
