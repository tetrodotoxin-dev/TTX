// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_FORM_STORAGE_H
#define TTX_DATA_FORM_STORAGE_H

#include "ttx/data/form/representation.h"

// Storage describes a buffer used to send or receive a value. Its
// Representation gives the value's layout and required alignment. `size` is
// the number of available bytes at `data`, which can exceed the value's extent.
// Transfer can then check that two buffers describe compatible values and have
// enough space before copying. The operation receiving Storage determines
// whether it reads or writes the buffer.
//
// Copying Storage copies the record, leaving the buffer and description in
// place. The caller must keep the buffer, Representation and descriptor bytes
// accessible for every use. Copying the record does not extend their lifetime.
typedef struct ttx_storage {
  const ttx_representation* representation;
  U8* data;
  Count size;
} ttx_storage;

// Checks capacity and data alignment using an already canonical Representation.
// A missing representation returns Invalid. A present representation must have
// readable descriptor bytes satisfying its decoding contract. Insufficient
// capacity returns Bounds. Nonempty values require a nonnull data address, and
// a misaligned address returns Invalid. Zero extent permits null data.
//
// This checks declared geometry. The caller establishes actual memory access
// and keeps the described region accessible through the operation.
C_LINKAGE EXPORTED(TTX)
ttx_data_status ttx_storage_check(ttx_storage storage);

#ifdef __cplusplus

#include "perimortem/core/access/bytes.hpp"
#include "perimortem/core/data.hpp"

#include "ttx/data/form/compiled.hpp"

namespace Ttx::Data::Form {

// Storage describes a buffer for transfer and negotiation through the C ABI.
// Its Representation supplies the value's layout, while the byte count gives
// the buffer's available capacity. `create` checks capacity and alignment.
// Construction from a C record leaves those checks to its caller or the
// receiving operation.
//
// The caller must keep the Representation object, descriptor bytes and buffer
// accessible throughout every use. Copies preserve their referenced addresses
// and existing access durations.
class Storage {
 public:
  // Uses a canonical Representation and the writable range supplied by Access.
  // Returns the geometry check's status on failure. The caller must keep the
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

  // Provides a View over the full declared capacity, including bytes beyond the
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

#endif

#endif
