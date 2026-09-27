// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/null_terminated.hpp"

#include "ttx/semantic/transport/flow.hpp"

#include "toolchain/validation/benchmark.hpp"

#include "perimortem/core/diagnostics/log.hpp"

#include "ttx/semantic/flows/copy.hpp"
#include "ttx/semantic/transport/direct.hpp"

using namespace Perimortem;
using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;
using namespace Ttx::Semantic::Negotiation;
using namespace Ttx::Semantic::Transport;

static constexpr auto integer = Schema::primitive(Schema::Value::U32);
static constexpr auto range = Schema::range(integer, 512, 4, 2048, 4);
static const auto& form = Compiled<range>::get_representation();
static U32 source[512];
static U32 destination[512];
static const Direct::Access::Operations operations = {
  [](const void*) -> const Representation* { return &form; },
  [](const void* source) -> const void* { return source; },
};
static const Query writer({
  source,
  [](const void* source, perimortem_uuid id, ttx_storage requested)
      -> ttx_binding_status {
    if (System::Uuid(id) != Direct::Access::contract_id) {
      return TTX_BINDING_UNSUPPORTED;
    }
    return static_cast<ttx_binding_status>(Binding::provide<Direct::Access>(
        Direct::Access::Api(source, &operations), Storage(requested)));
  },
  [](const void*, perimortem_uuid id) -> ttx_binding_status {
    return System::Uuid(id) == Direct::Access::contract_id
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_UNSUPPORTED;
  },
});
static Flow flow;
static const Storage output(
    ttx_storage{
      &form, reinterpret_cast<U8*>(destination), sizeof(destination)});
static Toolchain::Validation::Harness Transfer = {
  .name = "TTX::Transfer",
  .init =
      [] {
        source[511] = 42;
        if (flow.connect(Flow::reader(form), writer) != Flow::Status::Success) {
          Diagnostics::Log::fatal("Benchmark Flow failed to connect."_view);
        }
      },
};

// Establishment and repeated transfer are separate costs. The warm case
// borrows the same admitted representation and supplies its own destination.
VALIDATION_BENCHMARK(Transfer, establish_direct) {
  Flow candidate;
  auto status = candidate.connect(Flow::reader(form), writer);
  Toolchain::Validation::Benchmark::prevent_optimization(status);
}

VALIDATION_BENCHMARK(Transfer, copy_2k) {
  auto status = Ttx::Semantic::Flows::Copy::flow(flow, output);
  Toolchain::Validation::Benchmark::prevent_optimization(status);
  Toolchain::Validation::Benchmark::prevent_optimization(destination[511]);
}
