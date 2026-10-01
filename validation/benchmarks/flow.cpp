// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/abi/receiver.hpp"
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

// The consumer and provider each offer exactly one protocol. Establishment
// therefore measures its ordinary preference search, including the earlier
// unsupported requests. All four supply the same payload so transfer cost
// changes only with access.
static auto writer(Endpoint& endpoint) -> Query {
  return Query(ttx_semantic_query(
      &endpoint,
      [](void* source, perimortem_uuid contract,
         ttx_storage requested) -> ttx_binding_status {
        const auto& endpoint = Ttx::Abi::Receiver::get<Endpoint>(source);
        const System::Uuid id(contract);
        if (endpoint.protocol == Flow::Protocol::Direct &&
            id == Ttx::Semantic::Transport::Flow::direct.provider) {
          static const Ttx::Data::Protocol::Direct::Provider::Operations
              operations = Ttx::Data::Protocol::Direct::Provider::Operations(
                  representation, [](void* source) -> const void* {
                    return Ttx::Abi::Receiver::get<Endpoint>(source)
                        .values.get_data();
                  });
          return static_cast<ttx_binding_status>(
              Binding::provide<Ttx::Data::Protocol::Direct::Provider>(
                  Ttx::Data::Protocol::Direct::Provider::Api(
                      source, &operations),
                  Storage(requested)));
        } else if (
            endpoint.protocol == Flow::Protocol::Shared &&
            id == Ttx::Semantic::Transport::Flow::shared.provider) {
          static const Ttx::Data::Protocol::Shared::Provider::Operations
              operations = Ttx::Data::Protocol::Shared::Provider::Operations(
                  representation,
                  [](void* source,
                     ttx_shared_lifetime* result) -> ttx_data_status {
                    *result = {
                      Ttx::Abi::Receiver::get<Endpoint>(source).values.get_data(),
                      source,
                      [](void*) {},
                    };
                    return TTX_DATA_SUCCESS;
                  });
          return static_cast<ttx_binding_status>(
              Binding::provide<Ttx::Data::Protocol::Shared::Provider>(
                  Ttx::Data::Protocol::Shared::Provider::Api(
                      source, &operations),
                  Storage(requested)));
        } else if (
            endpoint.protocol == Flow::Protocol::Block &&
            id == Ttx::Semantic::Transport::Flow::block.provider) {
          static const Ttx::Data::Protocol::Block::Provider::Operations
              operations = Ttx::Data::Protocol::Block::Provider::Operations(
                  representation,
                  [](void* source,
                     ttx_storage target) -> ttx_data_status {
                    memmove(
                        target.data,
                        Ttx::Abi::Receiver::get<Endpoint>(source).values.get_data(),
                        form.get_extent());
                    return TTX_DATA_SUCCESS;
                  });
          return static_cast<ttx_binding_status>(
              Binding::provide<Ttx::Data::Protocol::Block::Provider>(
                  Ttx::Data::Protocol::Block::Provider::Api(
                      source, &operations),
                  Storage(requested)));
        } else if (
            endpoint.protocol == Flow::Protocol::Fragment &&
            id == Ttx::Semantic::Transport::Flow::fragment.provider) {
          static const Ttx::Data::Protocol::Fragment::Provider::Operations
              operations = {
                .representation = representation,
                .get_u32 = [](void* source, Count position,
                              U32* result) -> ttx_data_status {
                  *result = Ttx::Abi::Receiver::get<Endpoint>(source)
                                .values[position / 4];
                  return TTX_DATA_SUCCESS;
                },
              };
          return static_cast<ttx_binding_status>(
              Binding::provide<Ttx::Data::Protocol::Fragment::Provider>(
                  Ttx::Data::Protocol::Fragment::Provider::Api(
                      source, &operations),
                  Storage(requested)));
        }

        return TTX_BINDING_UNKNOWN;
      },
      [](void* source, perimortem_uuid contract) -> ttx_binding_status {
        const System::Uuid id(contract);
        switch (Ttx::Abi::Receiver::get<Endpoint>(source).protocol) {
        case Flow::Protocol::Direct:
          return id == Ttx::Semantic::Transport::Flow::direct.provider
                     ? TTX_BINDING_SATISFIED
                     : TTX_BINDING_UNKNOWN;
        case Flow::Protocol::Shared:
          return id == Ttx::Semantic::Transport::Flow::shared.provider
                     ? TTX_BINDING_SATISFIED
                     : TTX_BINDING_UNKNOWN;
        case Flow::Protocol::Block:
          return id == Ttx::Semantic::Transport::Flow::block.provider
                     ? TTX_BINDING_SATISFIED
                     : TTX_BINDING_UNKNOWN;
        case Flow::Protocol::Fragment:
          return id == Ttx::Semantic::Transport::Flow::fragment.provider
                     ? TTX_BINDING_SATISFIED
                     : TTX_BINDING_UNKNOWN;
        default:
          return TTX_BINDING_UNKNOWN;
        }
      }));
}

static Toolchain::Validation::Harness Transfer = {
  .name = "TTX::Transfer",
  .init =
      [] {
        for (Count i = 0; i < 4; ++i) {
          endpoints[i].values[511] = 42;
          const auto connected =
              flows[i].connect(Flow::consumer(form), writer(endpoints[i]));
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
  const auto source = writer(endpoints[index]);
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
