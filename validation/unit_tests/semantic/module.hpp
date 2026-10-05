// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "validation/support/library.hpp"

#include "perimortem/core/null_terminated.hpp"

#include "ttx/semantic/flows/swizzle.hpp"
#include "validation/providers/flow/provider.h"
#include "validation/providers/heterogeneous/provider.h"

namespace Validation::FlowTests {

// The shared library owns the code behind every returned function. Its scope
// encloses the provider states and retained Flows in each example.
class Module {
 public:
  using State = provider_state;
  using Heterogeneous = heterogeneous_state;
  using Primitives = provider_values;

  explicit Module(
      Perimortem::Core::View::Bytes library = "libflow_provider.so"_view,
      Perimortem::Core::View::Bytes entry = "flow_provider_open"_view);
  Module(const Module&) = delete;
  auto operator=(const Module&) -> Module& = delete;

  auto provider(State& state) const -> Ttx::Semantic::Negotiation::Query;

  auto provider(Heterogeneous& state) const
      -> Ttx::Semantic::Negotiation::Query;

  auto bootstrap_provider() const -> Ttx::Semantic::Negotiation::Query;

  auto import_query(const Ttx::Semantic::Transport::Flow& bootstrap) const
      -> Ttx::Semantic::Negotiation::Query;
  auto selection() const -> Ttx::Semantic::Flows::Swizzle::Mapping;

  auto primitives() const -> Ttx::Semantic::Negotiation::Query;

  auto primitive_representation() const
      -> const Ttx::Data::Form::Representation&;

  auto select(
      const Ttx::Semantic::Transport::Flow& flow,
      Ttx::Data::Form::Storage target) const -> Ttx::Data::Status;
  auto is_set() const -> Bool {
    return api != nullptr || heterogeneous != nullptr;
  }

 private:
  Perimortem::System::Library module;
  const provider_api* api = nullptr;
  const heterogeneous_provider* heterogeneous = nullptr;
};
}  // namespace Validation::FlowTests
