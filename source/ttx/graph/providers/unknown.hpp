// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"

namespace Ttx::Graph::Providers {

// Unknown supplies a shared Abstract for unresolved selections. Each handle
// refers to the same provider for the lifetime of the loaded TTX library.
// A fresh lookup at the original location can supply a different target
// while every route from the shared Unknown continues to return Unknown.
class Unknown {
 public:
  static EXPORTED(TTX) auto get_abstract() -> Abstract;

 private:
  Unknown() = default;
  Unknown(const Unknown&) = delete;
};

}  // namespace Ttx::Graph::Providers
