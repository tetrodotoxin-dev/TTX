// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#include "perimortem/core/diagnostics/log.hpp"

#include "perimortem/core/null_terminated.hpp"

#include "tests/semantic/fixtures.hpp"

#include <string.h>
using namespace Validation::FlowTests;

Module::Module(const char* library, const char* entry_name)
    : module(
          Validation::open_library(
              Perimortem::Core::NullTerminated::to_view(library))) {
  auto entry = Validation::find_symbol(
      module, Perimortem::Core::NullTerminated::to_view(entry_name));
  if (!strcmp(entry_name, "flow_provider_open")) {
    api = reinterpret_cast<
        const provider_api* (*)(decltype(&ttx_representation_compile))>(entry)(
        ttx_representation_compile);
  } else {
    heterogeneous = reinterpret_cast<
        const heterogeneous_provider* (*)(decltype(&ttx_representation_compile))>(
        entry)(ttx_representation_compile);
  }
}

auto Module::writer(State& state) const -> Query {
  return Query(api->writer(&state));
}

auto Module::bootstrap_writer() const -> Query {
  return Query(api->bootstrap_writer());
}
// This is the explicit Direct/Shared cast permission, not a cast of opaque
// binding source state. The host checked the C Query schema before this call.
auto Module::import_query(const Flow& flow) const -> Query {
  const auto cast = [](const void* pointer) {
    return Query(*static_cast<const ttx_semantic_query*>(pointer));
  };
  return flow.visit(
      cast, cast,
      [](auto, auto) -> Query {
        Diagnostics::Log::fatal("Bootstrap requires a castable protocol."_view);
      },
      [](auto) -> Query {
        Diagnostics::Log::fatal("Bootstrap requires a castable protocol."_view);
      });
}
auto Module::selection() const -> Swizzle::Mapping {
  return Swizzle::Mapping::create(*api->selection())
      .visit(
          [](auto& value) { return Perimortem::Core::Data::take(value); },
          [](Status) -> Swizzle::Mapping {
            Diagnostics::Log::fatal("Invalid C selection policy."_view);
          });
}
auto Module::select(const Flow& flow, Storage target) const -> Status {
  const provider_operations operations = {ttx_swizzle};
  const auto result =
      api->select(&operations, flow.get_abi(), target.get_abi());
  return static_cast<Status>(result);
}

auto Module::writer(Heterogeneous& state) const -> Query {
  return Query(heterogeneous->writer(&state));
}

auto Module::primitives() const -> Query {
  return Query(api->primitives());
}
auto Module::primitive_schema() const -> const Representation& {
  return *api->primitive_schema();
}
