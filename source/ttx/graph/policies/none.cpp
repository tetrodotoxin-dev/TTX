// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/none.h"

#include "ttx/graph/providers/none.hpp"

using namespace Ttx::Graph;

auto Policies::None::get_none() -> None {
  return None(Providers::None::get_abstract());
}

C_LINKAGE auto ttx_none(void) -> ttx_abstract {
  return Providers::None::get_abstract();
}
