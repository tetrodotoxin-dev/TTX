// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/unknown.hpp"

#include "ttx/graph/providers/unknown.hpp"

using namespace Ttx::Graph;

auto Policies::Unknown::get_unknown() -> Unknown {
  return Unknown(Providers::Unknown::get_abstract());
}

C_LINKAGE auto ttx_unknown(void) -> ttx_abstract {
  return Providers::Unknown::get_abstract();
}
