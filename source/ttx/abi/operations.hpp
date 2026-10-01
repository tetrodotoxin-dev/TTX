// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/policies/borrowed.h"

namespace Ttx::Abi {

// A typed capability facade may recover the complete table from its Abstract
// prefix. That facade's admitted C record establishes the table type. A bare
// Abstract does not prove which larger table, if any, owns its operations.
class Operations {
 public:
  template <typename Table>
  static auto from_abstract(const ttx_abstract_ops& abstract)
      -> const Table& {
    static_assert(__is_standard_layout(Table));
    static_assert(__builtin_offsetof(Table, abstract) == 0);
    return *reinterpret_cast<const Table*>(&abstract);
  }

  template <typename Table>
  static auto from_borrowed(const ttx_borrowed_ops& borrowed)
      -> const Table& {
    static_assert(__is_standard_layout(Table));
    static_assert(__builtin_offsetof(Table, borrowed) == 0);
    return *reinterpret_cast<const Table*>(&borrowed);
  }
};

}  // namespace Ttx::Abi
