// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/access/bytes.hpp"

#include "ttx/data/form/compiled.hpp"
#include "ttx/data/form/representation.hpp"
#include "ttx/data/form/storage.h"

namespace Ttx::Data::Form {

// Storage borrows a represented region. `create` checks its geometry;
// construction from a C record uses the caller's admitted representation.
class Storage {
 public:
  static auto create(
      const Representation& representation,
      Perimortem::Core::Access::Bytes bytes)
      -> Perimortem::Core::Storage::Result<Storage, Status> {
    const ttx_storage value =
        ttx_storage(&representation, bytes.get_data(), bytes.get_size());

    const auto status = ttx_storage_check(value);
    if (status) {
      return static_cast<Status>(status);
    }

    return Storage(value);
  }

  // The caller may already have admitted the supplied region. This constructor
  // consumes that established precondition instead of checking it again.
  constexpr explicit Storage(ttx_storage value) : value(value) {}

  // Native values supply their own declared layout and extent. This borrows
  // that value's storage, so it remains available through the operation.
  template <typename Value>
  static auto from(Value& data) -> Storage {
    return Storage(ttx_storage(
        &Compiled<Native<Value>::reference>::get_representation(),
        reinterpret_cast<U8*>(&data), sizeof(Value)));
  }

  constexpr auto get_representation() const -> const Representation& {
    return *value.representation;
  }

  // Observation includes the empty region. Operations receiving this Storage
  // establish writable access through its admitted destination contract.
  constexpr auto get_bytes() const -> Perimortem::Core::View::Bytes {
    return Perimortem::Core::View::Bytes(
        perimortem_view_bytes{value.data, value.size});
  }

  constexpr auto get_abi() const -> ttx_storage { return value; }

 private:
  ttx_storage value;
};
}  // namespace Ttx::Data::Form

TTX_DATA_RECORD(
    ttx_storage,
    TTX_DATA_MEMBER(ttx_storage, representation),
    TTX_DATA_MEMBER(ttx_storage, data),
    TTX_DATA_MEMBER(ttx_storage, size));
