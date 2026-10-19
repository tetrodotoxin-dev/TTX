// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/storage/bytes.hpp"

#include "ttx/data/form/compiler.hpp"
#include "ttx/data/form/representation.hpp"

namespace Ttx::Data::Form {

// A static Schema can be normalized during C++ translation so its consumers
// need no runtime compilation. Compiled publishes the same bytes as the runtime
// compiler and keeps them for the lifetime of its module. Preparation storage
// is discarded after constant evaluation.
//
// C++ needs the output array's size before constructing it. One preparation
// determines that size without writing bytes, then a second prepares and fills
// the array. Repeating preparation lets both passes use the runtime compiler's
// algorithm without a separate implementation for measuring constant schemas.
template <const auto& source, Count pointer_size = sizeof(void*)>
class Compiled {
 private:
  static consteval auto measure() -> Count {
    Compiler<Perimortem::Memory::Compile::Vector> compiler;
    const auto status = compiler.compile(source, pointer_size);
    return status == Status::Success ? compiler.get_size() : 0;
  }

  static constexpr Count size = measure();

  static_assert(size != 0, "Invalid constant TTX Schema");

  struct Storage {
    alignas(8) Perimortem::Core::Storage::Bytes<size> bytes;
    Representation representation;

    consteval Storage() : representation(bytes.get_data(), size) {
      // Measurement compiled this exact immutable source and established the
      // output extent. The repeated preparation follows the same algorithm,
      // so writing the canonical form needs no second failure state or
      // allocation policy.
      Compiler<Perimortem::Memory::Compile::Vector> compiler;
      compiler.compile(source, pointer_size);
      compiler.write(bytes.get_access());
    }
  };

 public:
  static constexpr auto get_representation() -> const Representation& {
    static constexpr Storage storage;
    return storage.representation;
  }
};

}  // namespace Ttx::Data::Form
