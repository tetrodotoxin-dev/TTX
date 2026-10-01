// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/form/representation.hpp"
#include "ttx/data/protocol/shared/lifetime.h"

namespace Ttx::Data::Protocol::Shared {

// Moving transfers one release obligation. `take_abi` clears this Lifetime
// before returning the C record, so release callbacks can reenter without
// releasing the same acquisition twice.
class Lifetime {
 public:
  constexpr Lifetime() : value() {}

  constexpr explicit Lifetime(ttx_shared_lifetime value) : value(value) {}

  Lifetime(const Lifetime&) = delete;

  auto operator=(const Lifetime&) -> Lifetime& = delete;

  constexpr Lifetime(Lifetime&& other) : value(other.take_abi()) {}

  auto operator=(Lifetime&& other) -> Lifetime& {
    if (this != &other) {
      ttx_shared_release(&value);
      value = other.take_abi();
    }

    return *this;
  }

  ~Lifetime() { ttx_shared_release(&value); }

  auto get_pointer() const -> const void* { return value.data; }

  auto take_abi() -> ttx_shared_lifetime {
    auto result = value;
    value = ttx_shared_lifetime();
    return result;
  }

 private:
  ttx_shared_lifetime value = ttx_shared_lifetime();
};

}  // namespace Ttx::Data::Protocol::Shared

TTX_DATA_RECORD(
    ttx_shared_lifetime,
    TTX_DATA_MEMBER(ttx_shared_lifetime, data),
    TTX_DATA_MEMBER(ttx_shared_lifetime, source),
    TTX_DATA_MEMBER(ttx_shared_lifetime, release));
