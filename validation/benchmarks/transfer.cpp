// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/transfer.hpp"

#include "toolchain/validation/benchmark.hpp"

#include "perimortem/core/diagnostics/log.hpp"
#include "perimortem/core/view/null_terminated.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;

static Toolchain::Validation::Harness TransferBenchmarks = {
  .name = "TTX::Transfer"};

static constexpr auto integer =
    Form::Schema::primitive(Form::Schema::Value::U32);
static constexpr auto sequence = Form::Schema::range(integer, 512, 4, 2048, 4);
static const auto& form = Form::Compiled<sequence>::get_representation();
alignas(64) static U32 source[512] = {};
alignas(64) static U32 destination[516] = {};

// Both destinations meet the value's four byte alignment. Offsetting one by
// four bytes changes its cache alignment while preserving its declared form.
// Each sample repeats the complete checked transfer of 2048 bytes 1024 times.
static void transfer(U32* destination) {
  const ttx_storage input{&form, reinterpret_cast<U8*>(source), sizeof(source)};
  const ttx_storage output{
    &form, reinterpret_cast<U8*>(destination), sizeof(source)};

  for (Count index = 0; index != 1024; ++index) {
    if (ttx_transfer(&input, &output) != TTX_DATA_SUCCESS) {
      Diagnostics::Log::fatal("Benchmark transfer failed."_view);
    }
  }
}

VALIDATION_BENCHMARK(TransferBenchmarks, aligned_2k) {
  transfer(destination);
}

VALIDATION_BENCHMARK(TransferBenchmarks, offset_2k) {
  transfer(destination + 1);
}
