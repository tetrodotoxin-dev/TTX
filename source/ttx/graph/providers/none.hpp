// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"

namespace Ttx::Graph::Providers {

// None supplies a shared Abstract for a route that has no target at this
// observation. Each handle refers to the same provider for the lifetime of the
// loaded TTX library. Independent providers can establish the same absence by
// supporting the None policy at their own locations.
class None {
 public:
  static EXPORTED(TTX) auto get_abstract() -> Abstract;

 private:
  None() = default;
  None(const None&) = delete;
};

}  // namespace Ttx::Graph::Providers
