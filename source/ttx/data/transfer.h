// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_TRANSFER_H
#define TTX_DATA_TRANSFER_H

#include "ttx/data/form/storage.h"

// Transfer copies one represented value after comparing its source and
// destination descriptions. The destination supplies a canonical form. The
// source descriptor bytes are readable for their declared size, so comparison
// can reject a different form before decoding its geometry.
//
// Both data regions require the represented extent and alignment. The caller
// supplies accessible memory for those declared regions. Transfer checks their
// declared capacities and addresses, then copies exactly the represented extent
// with overlap support. Spare destination capacity remains unchanged. A failed
// check leaves all destination bytes unchanged. A canonical form with zero data
// extent permits null data pointers.
//
// The call finishes synchronously. Copying a value copies its stored pointer
// values and preserves their existing access contracts. Referenced contexts,
// descriptions and functions stay accessible for the duration established by
// their suppliers. The participating contracts establish which payload bit
// patterns are meaningful and how those pointers may be used.
C_LINKAGE EXPORTED(TTX)
ttx_data_status ttx_transfer(
    const ttx_storage* input,
    const ttx_storage* output);

#ifdef __cplusplus

namespace Ttx::Data {

// Transfer copies a represented value through the C Data operation. The two
// Storage records supply source and destination geometry. Their referenced
// memory stays accessible until the synchronous copy returns. The operation
// copies only the represented extent after checking both regions. A failed
// check leaves destination bytes unchanged.
class Transfer {
 public:
  static auto copy(Form::Storage input, Form::Storage output) -> Status {
    const auto source = input.get_abi();
    const auto target = output.get_abi();
    return Status(ttx_transfer(&source, &target));
  }
};

}  // namespace Ttx::Data

#endif

#endif
