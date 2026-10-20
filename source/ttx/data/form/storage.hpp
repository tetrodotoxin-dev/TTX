// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/access/bytes.hpp"
#include "perimortem/core/data.hpp"

#include "ttx/data/form/compiled.hpp"
#include "ttx/data/form/representation.hpp"
#include "ttx/data/form/storage.h"

namespace Ttx::Data::Form {

// Storage supplies represented memory to Data and negotiation operations.
// It copies the C record while keeping the representation and data under their
// existing access contracts. `create` checks declared capacity and alignment.
// Construction from a C record stores it for an operation to check or use under
// geometry already established by the caller.
class Storage {
 public:
  // Uses a canonical Representation and the writable range supplied by Access.
  // Returns the geometry check's status on failure. The caller keeps the
  // Representation object, its descriptor bytes and the region accessible for
  // every use of the returned Storage.
  static auto create(
      const Representation& representation,
      Perimortem::Core::Access::Bytes bytes)
      -> Perimortem::Core::Storage::Result<Storage, Status> {
    const ttx_storage value = ttx_storage(
        &representation.get_abi(), bytes.get_data(), bytes.get_size());

    const auto status = ttx_storage_check(value);
    if (status) {
      return Status(status);
    }

    return Storage(value);
  }

  // Copies the record without validating its pointers, capacity or alignment.
  // Explicit construction keeps this unchecked path distinct from `create`.
  constexpr explicit Storage(ttx_storage value) : value(value) {}

  // Describes one writable native lvalue using Native<Value>. Its complete
  // object size supplies capacity. The caller keeps the value alive and at
  // the same address through every operation using this Storage.
  template <typename Value>
  static auto from(Value& data) -> Storage {
    return Storage(ttx_storage(
        &Compiled<Native<Value>::reference>::get_representation().get_abi(),
        Perimortem::Core::Data::cast<U8>(&data), sizeof(Value)));
  }

  constexpr auto get_representation() const -> Representation {
    return *value.representation;
  }

  // Observes the full declared capacity, including any bytes beyond the
  // represented value's extent. The caller supplies readable memory for that
  // capacity. Zero capacity produces the canonical empty View.
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
