// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/option.hpp"

namespace Validation::FlowTests {

// Performance assertions observe the caller's Bibliotheca checkouts and direct
// allocation and copy calls linked into this executable. Code inside a foreign
// library can allocate through its own runtime, so these counters make no
// claim about that provider's hidden work. Positive controls exercise each
// observed route before operation tests rely on a zero count. Windows observes
// Bibliotheca checkouts only. Its linker cannot wrap the executable's runtime
// calls, so copy measurements are unavailable there.
class Measurement {
 public:
  Measurement();
  ~Measurement();
  Measurement(const Measurement&) = delete;
  auto operator=(const Measurement&) -> Measurement& = delete;

  auto stop() -> void;
  auto get_allocations() const -> Count { return allocations; }
  auto get_copies() const -> Perimortem::Core::Option<Count>;
  static auto wraps_runtime() -> Bool;

  // Linker wrappers call these hooks. The examples observe counts rather than
  // replacing an allocator or a copy implementation with test behavior.
  static auto allocation() -> void;
  static auto copy() -> void;

 private:
  static Measurement* active;
  Measurement* previous;
  Count checkouts;
  Count allocations = 0;
  Count copies = 0;
};

}  // namespace Validation::FlowTests
