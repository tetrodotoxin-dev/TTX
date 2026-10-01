// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#include "ttx/data/protocol/shared/lifetime.hpp"

auto ttx_shared_release(ttx_shared_lifetime* lifetime) -> void {
  // Release enters provider code, which can reenter the consumer using this
  // record. Remove the old obligation first so that reentry cannot release it
  // twice.
  const auto previous = *lifetime;
  *lifetime = ttx_shared_lifetime();
  if (previous.release) {
    previous.release(previous.source);
  }
}
