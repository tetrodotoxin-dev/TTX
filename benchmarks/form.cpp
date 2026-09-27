// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/null_terminated.hpp"

#include "toolchain/validation/benchmark.hpp"

#include "perimortem/core/diagnostics/log.hpp"

#include "ttx/data/form/compiler.hpp"
#include "ttx/data/form/representation.hpp"

using namespace Perimortem;
using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static constexpr auto integer = Schema::primitive(Schema::Value::U32);
static constexpr auto real = Schema::primitive(Schema::Value::R32);
static Schema::Position positions[16384];
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
  const auto schema = Schema::composite({positions, count}, count * 4, 4);
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

static U8 publications[2][(16384 + 1) * 8];
static Representation first;
static Representation second;
static Toolchain::Validation::Harness Agreement = {
  .name = "TTX::Agreement",
  .init =
      [] {
        Forms.init();
        const auto schema = Schema::composite({positions, 16384}, 65536, 4);
        Compiler compiler;
        if (compiler.compile(schema) != Status::Success ||
            compiler.write(Access::Bytes(publications[0])) != Status::Success ||
            compiler.write(Access::Bytes(publications[1])) != Status::Success) {
          Diagnostics::Log::fatal(
              "Benchmark representation failed to compile."_view);
        }
        first = Representation(publications[0], compiler.get_size());
        second = Representation(publications[1], compiler.get_size());
      },
};

// Independent publications keep pointer identity from bypassing comparison.
VALIDATION_BENCHMARK(Agreement, compare_16384) {
  auto agrees = first.compatible(second);
  Toolchain::Validation::Benchmark::prevent_optimization(agrees);
}
