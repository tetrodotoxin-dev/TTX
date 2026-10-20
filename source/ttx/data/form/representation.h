// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_FORM_REPRESENTATION_H
#define TTX_DATA_FORM_REPRESENTATION_H

#include "ttx/data/form/schema.h"

// Navigation returns a primitive occurrence by value. Its byte coordinates
// belong to the containing object, while type and byte order describe the
// observation at that coordinate. No pointer to a reconstructed child object
// needs to survive the lookup.
// Pointer and callable slots both produce a Pointer observation whose extent
// comes from the representation's pointer width, including when the reader
// runs on another target. Navigation reports the pointer slot's coordinates.
// It reads the encoded descriptions rather than dereferencing addresses in the
// payload. Target descriptions participate in comparison of the complete form.
typedef struct ttx_representation_position {
  Count offset;
  ttx_schema_value type;
  U8 byte_order;
  U32 extent;

} ttx_representation_position;

// Composition places complete canonical forms inside a new struct. Each child
// retains its extent, padding and struct boundary. The source buffers need
// survive only composition because the new canonical form contains its own
// copies.
typedef struct ttx_representation_member {
  const struct ttx_representation* representation;
  Count offset;

} ttx_representation_member;

// A canonical form is the compiled description of data and callable layout.
// Compilation establishes the normalization and geometry needed for decoding.
// An independent provider can publish the same format by establishing the
// invariants specified in `compiler.hpp`:
//
//  * Its total byte size is a multiple of eight
//  * Any padding is required to be zeroed
//  * Any two representations are equal only if they are byte for byte equal
//
// Readers can therefore load whole U64 chunks relative to the buffer start
// without requiring padding at each block. Descriptor bytes support any byte
// alignment. The alignment encoded in the form applies to the represented
// payload. Geometry and navigation operations require canonical descriptor
// bytes. Decoding a noncanonical form is undefined behavior. Byte comparison
// only requires readable descriptor memory. Transfer compares a foreign source
// with the destination's established canonical form before interpreting source
// geometry.
//
// The provider keeps the descriptor bytes available and unchanged through each
// use. Compiler and Schema storage may end independently. References are block
// indices, so moving the bytes preserves their meaning.
typedef struct ttx_representation {
  const U8* data;
  Count size;

} ttx_representation;

// Compiler retains temporary storage during preparation. After preparation
// succeeds, it requests one final allocation from the caller's allocator. The
// caller retains that allocation, including the record and its encoded bytes.
// The allocator returns writable storage meeting the requested size and
// alignment, or null to report insufficient storage. Its context is used only
// during the compilation or composition call.
typedef struct ttx_representation_allocator {
  void* context;
  void* (*allocate)(void* context, Count bytes, Count alignment);
} ttx_representation_allocator;

// Compiles a stable Schema graph with four or eight byte pointer storage,
// independently of the host. Unsupported pointer widths return Unsupported
// before requesting final storage. The allocator receives one request after
// successful preparation. The compiler writes the new Representation to result
// only when returning Success. On failure it leaves that slot unchanged.
// The caller keeps the final
// allocation available for every use of the returned record and bytes.
C_LINKAGE EXPORTED(TTX)
ttx_data_status ttx_representation_compile(
    ttx_schema_reference schema,
    Count pointer_size,
    ttx_representation_allocator allocator,
    const ttx_representation** result);

// Composes count canonical child forms at their member offsets. Children remain
// readable through the call. A zero count permits a null members pointer and
// can describe an empty result. Extent, alignment and placement are checked
// before the final allocation. The compiler writes an independent canonical
// form to result only when returning Success. On failure it leaves that slot
// unchanged.
C_LINKAGE EXPORTED(TTX)
ttx_data_status ttx_representation_compose(
    const ttx_representation_member* members,
    Count count,
    Count extent,
    Count alignment,
    ttx_representation_allocator allocator,
    const ttx_representation** result);

// Compares readable descriptor bytes. A null record pointer returns false.
// Canonical forms that compare equal describe the same physical layout and
// calling convention. The supplying contracts establish their semantic meaning.
C_LINKAGE EXPORTED(TTX)
U8 ttx_representation_compatible(
    const ttx_representation* source,
    const ttx_representation* destination);

// Finds the first primitive starting at or after offset in a canonical form.
// Padding and a partially selected primitive advance to the next start. Bounds
// means no such start exists. The operation writes result only when returning
// Success. Null source or result pointers return Invalid. Present descriptor
// memory satisfies the form's decoding contract.
C_LINKAGE EXPORTED(TTX)
ttx_data_status ttx_representation_next(
    const ttx_representation* source,
    Count offset,
    ttx_representation_position* result);

// C operations need the same streaming walk as native consumers. The callback
// receives each primitive's physical coordinate and type while the walker
// retains its current path on the stack. Returning a failure stops before the
// next occurrence. No callback state or position pointer survives the call, so
// consuming a record needs neither an allocated iterator nor repeated searches.
typedef struct ttx_representation_visitor {
  void* context;
  ttx_data_status (*visit)(void* context, ttx_representation_position position);
} ttx_representation_visitor;

C_LINKAGE EXPORTED(TTX)
ttx_data_status ttx_representation_visit(
    const ttx_representation* source,
    ttx_representation_visitor visitor);

// The coordinates are a borrowed, strictly increasing selection of primitive
// starts. Unselected repetitions consume no callbacks or expanded inventory.
C_LINKAGE EXPORTED(TTX)
ttx_data_status ttx_representation_visit_selected(
    const ttx_representation* source,
    const Count* coordinates,
    Count count,
    ttx_representation_visitor visitor);

#endif
