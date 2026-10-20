// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/benchmark.hpp"

#include "perimortem/core/diagnostics/log.hpp"
#include "perimortem/core/storage/vector.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/data/form/compiler.hpp"
#include "ttx/data/form/representation.hpp"

using namespace Perimortem;
using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static constexpr auto integer = Schema::primitive(Schema::Value::U32);
static constexpr auto real = Schema::primitive(Schema::Value::R32);
static Perimortem::Core::Storage::Vector<Schema::Position, 16384> positions;
static Toolchain::Validation::Harness Forms = {
  .name = "TTX::Compiler",
  .init =
      [] {
        for (Count i = 0; i < 16384; ++i) {
          positions[i] = Schema::Position(i % 2 ? integer : real, i * 4);
        }
      },
};

// Alternating types retain every descriptor. Report several widths so a
// compact homogeneous run cannot hide the cost of compiling a wide record.
static auto compile(Count count) -> void {
  const auto schema = Schema::composite(
      {
        positions[0],
        count,
      },

      count * 4, 4);
  Compiler compiler;
  if (compiler.compile(schema) != Status::Success) {
    Diagnostics::Log::fatal("Benchmark schema failed to compile."_view);
  }

  auto bytes = compiler.get_size();
  Toolchain::Validation::Benchmark::prevent_optimization(bytes);
}

VALIDATION_BENCHMARK(Forms, compile_512) {
  compile(512);
}

VALIDATION_BENCHMARK(Forms, compile_4096) {
  compile(4096);
}

VALIDATION_BENCHMARK(Forms, compile_16384) {
  compile(16384);
}

static Perimortem::Core::Storage::
    Vector<Perimortem::Core::Storage::Vector<U8, (16384 + 1) * 8>, 2>
        publications;
static Representation first;
static Representation second;
static Toolchain::Validation::Harness Agreement = {
  .name = "TTX::Agreement",
  .init =
      [] {
        Forms.init();

        const auto schema = Schema::composite(
            {
              positions[0],
              16384,
            },

            65536, 4);
        Compiler compiler;
        if (compiler.compile(schema) != Status::Success ||
            compiler.write(
                Access::Bytes(
                    publications[0][0], publications[0].get_size())) !=
                Status::Success ||
            compiler.write(
                Access::Bytes(
                    publications[1][0], publications[1].get_size())) !=
                Status::Success) {
          Diagnostics::Log::fatal(
              "Benchmark representation failed to compile."_view);
        }

        first = Representation(publications[0].get_data(), compiler.get_size());
        second =
            Representation(publications[1].get_data(), compiler.get_size());
      },
};

// The two forms have separate byte buffers. Comparison must read those bytes
// instead of returning early because their addresses match.
VALIDATION_BENCHMARK(Agreement, compare_16384) {
  auto agrees = first.compatible(second);
  Toolchain::Validation::Benchmark::prevent_optimization(agrees);
}
