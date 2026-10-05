// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/transport/flow.hpp"

#include "perimortem/core/static/vector.hpp"
#include "perimortem/core/diagnostics/log.hpp"
#include "perimortem/core/null_terminated.hpp"

#include "toolchain/validation/benchmark.hpp"
#include "ttx/data/protocol/block/provider.hpp"
#include "ttx/data/protocol/direct/provider.hpp"
#include "ttx/data/protocol/fragment/provider.hpp"
#include "ttx/data/protocol/shared/provider.hpp"
#include "ttx/semantic/flows/copy.hpp"

using namespace Perimortem;
using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;
using namespace Ttx::Semantic::Negotiation;
using namespace Ttx::Semantic::Transport;

static constexpr auto integer = Schema::primitive(Schema::Value::U32);
static constexpr auto range = Schema::range(integer, 512, 4, 2048, 4);
static const auto& form = Compiled<range>::get_representation();
static constexpr Count repetitions = 1024;

struct Endpoint {
  Flow::Protocol protocol;
  alignas(64) Static::Vector<U32, 512> values;
};

static Static::Vector<Endpoint, 4> endpoints = {
  {
    Endpoint(Flow::Protocol::Direct),
    {
      Flow::Protocol::Shared,
    },
    {
      Flow::Protocol::Block,
    },
    {
      Flow::Protocol::Fragment,
    },
  },
};
static Static::Vector<Flow, 4> flows;

// Explicit alignment keeps linker placement from selecting a different native
// copy path after unrelated symbols change. The second target deliberately
// starts sixteen bytes into the same aligned region.
alignas(64) static Static::Vector<U32, 516> destination;

static const Storage output(
    ttx_storage(&form, reinterpret_cast<U8*>(destination.get_data()), 2048));
static const Storage unaligned_output(ttx_storage(
    &form,
    reinterpret_cast<U8*>(destination.get_data() + 4),
    2048));

static auto representation(void*) -> const Representation* {
  return &form;
}

static auto read(void* context) -> const void* {
  return static_cast<Endpoint*>(context)->values.get_data();
}

static void release(void*) {}

static auto acquire(void* context, ttx_shared_lifetime* output)
    -> ttx_data_status {
  *output = {read(context), context, release};
  return TTX_DATA_SUCCESS;
}

static auto commit(void* context, ttx_storage output) -> ttx_data_status {
  memmove(output.data, read(context), form.get_extent());
  return TTX_DATA_SUCCESS;
}

static auto fragment(void* context, Count position, U32* output)
    -> ttx_data_status {
  *output = static_cast<Endpoint*>(context)->values[position / 4];
  return TTX_DATA_SUCCESS;
}

static auto supports(void* context, perimortem_uuid contract)
    -> ttx_binding_status {
  const auto& endpoint = *static_cast<Endpoint*>(context);
  const System::Uuid id(contract);
  switch (endpoint.protocol) {
  case Flow::Protocol::Direct:
    return id == Flow::direct.provider ? TTX_BINDING_SATISFIED
                                       : TTX_BINDING_UNKNOWN;
  case Flow::Protocol::Shared:
    return id == Flow::shared.provider ? TTX_BINDING_SATISFIED
                                       : TTX_BINDING_UNKNOWN;
  case Flow::Protocol::Block:
    return id == Flow::block.provider ? TTX_BINDING_SATISFIED
                                      : TTX_BINDING_UNKNOWN;
  case Flow::Protocol::Fragment:
    return id == Flow::fragment.provider ? TTX_BINDING_SATISFIED
                                         : TTX_BINDING_UNKNOWN;
  default:
    return TTX_BINDING_UNKNOWN;
  }
}

static auto bind(void* context, perimortem_uuid contract, ttx_storage requested)
    -> ttx_binding_status {
  if (supports(context, contract) != TTX_BINDING_SATISFIED) {
    return TTX_BINDING_UNKNOWN;
  }

  const auto& endpoint = *static_cast<Endpoint*>(context);
  const Storage output(requested);
  switch (endpoint.protocol) {
  case Flow::Protocol::Direct: {
    using Provider = Ttx::Data::Protocol::Direct::Provider;
    static const Provider::Operations operations = {representation, read};
    return static_cast<ttx_binding_status>(
        Binding::provide<Provider>({context, &operations}, output));
  }

  case Flow::Protocol::Shared: {
    using Provider = Ttx::Data::Protocol::Shared::Provider;
    static const Provider::Operations operations = {representation, acquire};
    return static_cast<ttx_binding_status>(
        Binding::provide<Provider>({context, &operations}, output));
  }

  case Flow::Protocol::Block: {
    using Provider = Ttx::Data::Protocol::Block::Provider;
    static const Provider::Operations operations = {representation, commit};
    return static_cast<ttx_binding_status>(
        Binding::provide<Provider>({context, &operations}, output));
  }

  case Flow::Protocol::Fragment: {
    using Provider = Ttx::Data::Protocol::Fragment::Provider;
    static const Provider::Operations operations = {
      .representation = representation, .get_u32 = fragment};
    return static_cast<ttx_binding_status>(
        Binding::provide<Provider>({context, &operations}, output));
  }

  default:
    return TTX_BINDING_UNKNOWN;
  }
}

// Both sides offer one protocol. Establishment includes the preceding refused
// requests. All four protocols read the same payload.
static auto provider(Endpoint& endpoint) -> Query {
  return Query({&endpoint, bind, supports});
}

static Toolchain::Validation::Harness Transfer = {
  .name = "TTX::Transfer",
  .init =
      [] {
        for (Count i = 0; i < 4; ++i) {
          endpoints[i].values[511] = 42;

          const auto connected =
              flows[i].connect(Flow::consumer(form), provider(endpoints[i]));
          if (connected != Flow::Status::Success ||
              flows[i].get_protocol() != endpoints[i].protocol) {
            Diagnostics::Log::fatal("Benchmark Flow failed to connect."_view);
          }

          const auto copied =
              Ttx::Semantic::Flows::Copy::flow(flows[i], output);
          if (copied != Status::Success || destination[511] != 42) {
            Diagnostics::Log::fatal("Benchmark Flow failed to copy."_view);
          }

          destination[511] = 0;
        }
      },
};

// Batching keeps clock overhead small relative to these short operations.
// Reported times cover 1024 establishments or 1024 copies of 2048 bytes.
// Establishment includes closing its acquired lifetime, while warm copies
// reuse one agreement and keep all negotiation outside their timed loop.
static auto establish(Count index) -> void {
  const auto source = provider(endpoints[index]);
  const auto target = Flow::consumer(form);
  for (Count i = 0; i < repetitions; ++i) {
    Flow candidate;
    auto status = candidate.connect(target, source);
    Toolchain::Validation::Benchmark::prevent_optimization(status);
  }
}

static auto copy(Count index, const Storage& target = output) -> void {
  for (Count i = 0; i < repetitions; ++i) {
    auto status = Ttx::Semantic::Flows::Copy::flow(flows[index], target);
    Toolchain::Validation::Benchmark::prevent_optimization(status);
    Toolchain::Validation::Benchmark::prevent_optimization(
        target.get_bytes().get_data()[2044]);
  }
}

VALIDATION_BENCHMARK(Transfer, establish_direct) {
  establish(0);
}

VALIDATION_BENCHMARK(Transfer, establish_shared) {
  establish(1);
}

VALIDATION_BENCHMARK(Transfer, establish_block) {
  establish(2);
}

VALIDATION_BENCHMARK(Transfer, establish_fragment) {
  establish(3);
}

VALIDATION_BENCHMARK(Transfer, copy_direct_2k) {
  copy(0);
}

VALIDATION_BENCHMARK(Transfer, copy_shared_2k) {
  copy(1);
}

VALIDATION_BENCHMARK(Transfer, copy_block_2k) {
  copy(2);
}

VALIDATION_BENCHMARK(Transfer, copy_fragment_2k) {
  copy(3);
}

VALIDATION_BENCHMARK(Transfer, copy_direct_unaligned_2k) {
  copy(0, unaligned_output);
}

VALIDATION_BENCHMARK(Transfer, copy_shared_unaligned_2k) {
  copy(1, unaligned_output);
}

VALIDATION_BENCHMARK(Transfer, copy_block_unaligned_2k) {
  copy(2, unaligned_output);
}
