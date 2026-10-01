// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

namespace Ttx::Abi {

// An owning callback recovers only the native provider whose address it
// published as this receiver. The owning callback establishes that local type.
// A foreign receiver or its address alone proves no C++ type.
class Receiver {
 public:
  template <typename Provider>
  static auto get(void* source) -> Provider& {
    return *static_cast<Provider*>(source);
  }
};

}  // namespace Ttx::Abi
