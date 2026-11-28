// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_FORM_REPRESENTATION_H
#define TTX_DATA_FORM_REPRESENTATION_H

#include "ttx/data/form/schema.h"

// Navigation returns a primitive occurrence by value. Its offset, type and
// byte order describe one position within the represented object. Returning
// coordinates by value lets lookup finish independently of any reconstructed
// child object.
// Pointer and callable slots produce Pointer positions whose extents come from
// the representation's pointer width, including when the reader runs on another
// target. Navigation must derive these coordinates from descriptor bytes alone.
// Target descriptions participate in comparison of the complete form.
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

// Coordinates select primitive starts in strictly increasing order. The caller
// keeps the coordinate array readable until visitation returns. Unselected
// repetitions consume no callbacks or expanded inventory.
C_LINKAGE EXPORTED(TTX)
ttx_data_status ttx_representation_visit_selected(
    const ttx_representation* source,
    const Count* coordinates,
    Count count,
    ttx_representation_visitor visitor);

#ifdef __cplusplus

#include "perimortem/core/storage/result.hpp"
#include "perimortem/memory/global/bytes.hpp"

#include "ttx/data/encoding/element.hpp"
#include "ttx/data/encoding/struct.hpp"
#include "ttx/data/form/native.hpp"
#include "ttx/data/status.h"

namespace Ttx::Data::Form {

// Representation compares data descriptions and locates primitives by their
// byte offsets. Compiler serializes equivalent layouts and callable signatures
// to the same canonical bytes. Equality is therefore a byte for byte comparison
// without reconstructing a Schema graph. The encoding is specified in
// compiler.hpp, including its indices and zero padding to a multiple of eight
// bytes.
//
// Compilation and composition keep their descriptor Bytes in the returned
// Representation. Copying that Representation copies its Bytes into an
// independent allocation. Moving transfers the allocation.
//
// Construction from a C record or View stores its address and size. The caller
// keeps those bytes readable and unchanged through every use, including uses
// of copies. This path allocates nothing and supports embedded canonical forms.
// Geometry and navigation require canonical descriptor bytes. Decoding a
// noncanonical form is undefined behavior.
// Descriptor bytes support any byte alignment. The alignment described by
// those bytes applies to the payload being represented.
//
// Comparison reads both descriptor ranges without decoding their contents.
// Transfer can therefore compare a foreign source against an established
// canonical destination before using the source's encoded geometry.
class Representation {
 public:
  // Navigation returns a primitive occurrence by value. Its offset, type and
  // byte order describe one position within the represented object. Returning
  // coordinates by value lets lookup finish independently of any reconstructed
  // child object.
  // Pointer and callable slots produce Pointer positions whose extents come
  // from the representation's pointer width, including when the reader runs on
  // another target. Navigation must derive these coordinates from descriptor
  // bytes alone. Target descriptions participate in comparison of the complete
  // form.
  class Position {
   public:
    using Value = Schema::Value;
    using ByteOrder = Schema::ByteOrder;

    constexpr Position(
        Count offset = 0,
        Value type = Value::U8,
        ByteOrder byte_order = ByteOrder::Little,
        U32 extent = 0)
        : value{
            offset, U8(type), U8(byte_order),
            extent ? extent : U32(Schema::get_width(type))} {}

    constexpr auto get_value() const -> Value { return Value(value.type); }

    constexpr auto get_byte_order() const -> ByteOrder {
      return ByteOrder(value.byte_order);
    }

    constexpr auto get_extent() const -> Count { return value.extent; }

    constexpr auto compatible(const Position& other) const -> Bool {
      return value.type == other.value.type &&
             value.byte_order == other.value.byte_order &&
             value.extent == other.value.extent;
    }

    constexpr Position(ttx_representation_position value) : value(value) {}
    constexpr auto get_offset() const -> Count { return value.offset; }
    constexpr auto get_abi() const -> ttx_representation_position {
      return value;
    }

   private:
    ttx_representation_position value;
  };

  constexpr Representation(ttx_representation value) : value(value) {}
  using Member = ttx_representation_member;
  using Value = Schema::Value;
  using ByteOrder = Schema::ByteOrder;

  // Stores the descriptor address and byte count without validating the form.
  // The default null record can receive a later description. Geometry and
  // navigation require canonical descriptor bytes before use.
  constexpr Representation(const U8* data = nullptr, Count size = 0)
      : value{data, size} {}

  constexpr Representation(Perimortem::Core::View::Bytes view)
      : value{view.get_data(), view.get_size()} {}

  constexpr Representation(Perimortem::Memory::Global::Bytes&& bytes)
      : value{bytes.get_data(), bytes.get_size()},
        bytes(Perimortem::Core::Data::take(bytes)) {}

  constexpr Representation(const Representation& source) : value(source.value) {
    if (!source.bytes.is_empty()) {
      bytes = source.bytes;
      value = {bytes.get_data(), bytes.get_size()};
    }
  }

  constexpr Representation(Representation&& source)
      : value(source.value), bytes(Perimortem::Core::Data::take(source.bytes)) {
    source.value = {};
  }

  constexpr auto operator=(const Representation& source) -> Representation& {
    if (this != &source) {
      Representation copy(source);
      *this = Perimortem::Core::Data::take(copy);
    }

    return *this;
  }

  auto operator=(Representation&& source) -> Representation& {
    if (this != &source) {
      bytes = Perimortem::Core::Data::take(source.bytes);
      value = source.value;
      source.value = {};
    }

    return *this;
  }

  constexpr auto get_bytes() const -> Perimortem::Core::View::Bytes {
    return Perimortem::Core::View::Bytes(
        perimortem_view_bytes{value.data, value.size});
  }

  constexpr auto get_blocks() const -> Perimortem::Core::View::Bytes {
    const Count prefix = (value.data[0] & 15) ? 0 : 8;
    return get_bytes().slice(prefix, value.size - prefix);
  }

  constexpr auto get_pointer_size() const -> Count {
    return (value.data[0] & 15) ? 8 : 4;
  }

  // Canonical encoding supplies a complete root after any pointer width prefix.
  constexpr auto get_depth() const -> U8 {
    return get_blocks().get_data()[0] & 15;
  }

  constexpr auto get_extent() const -> Count;

  constexpr auto get_alignment() const -> Count;

  // C operations read this record and its descriptor bytes. The caller
  // keeps the Representation at the same address and unchanged while an
  // operation or Storage holds a pointer to the record.
  constexpr auto get_abi() const -> const ttx_representation& { return value; }

  // Compares descriptor size and bytes without decoding. For canonical forms,
  // equality establishes the same layout and calling convention. The caller
  // keeps both byte ranges readable. Meanings and payload values follow their
  // own contracts.
  constexpr auto compatible(const Representation& other) const -> Bool;

  // Compiles a stable Schema graph into a Representation that owns its
  // canonical descriptor Bytes. Source descriptions are needed only through
  // this call. Preparation failures return their status before allocating the
  // output buffer. The result keeps the complete form available independently
  // of the source graph.
  static EXPORTED(TTX) auto compile(
      ttx_schema_reference schema,
      Count pointer_size = sizeof(void*))
      -> Perimortem::Core::Storage::Result<Representation, Ttx::Data::Status>;

  // Builds a canonical struct from complete canonical child forms. Members
  // retain their extents and padding at the requested offsets. The children
  // remain readable through this call. The returned Representation owns its
  // descriptor Bytes independently of those children. Preparation failures
  // return their status before allocating the output buffer.
  static EXPORTED(TTX) auto compose(
      Perimortem::Core::View::Vector<Member> members,
      Count extent,
      Count alignment)
      -> Perimortem::Core::Storage::Result<Representation, Ttx::Data::Status>;

  // A byte coordinate selects the first primitive whose start is at or after
  // it. Padding is skipped, and a coordinate inside a primitive advances to
  // the following primitive rather than returning a partial value. Bounds
  // means there is no such start. Use visit when consuming a whole record so
  // each successive occurrence does not repeat this search from the root.
  EXPORTED(TTX) auto next(Count offset) const
      -> Perimortem::Core::Storage::Result<Position, Ttx::Data::Status>;

  // Whole operations consume primitive occurrences in byte order. Repeated
  // composites revisit their shared body for each instance, but successive
  // occurrences never restart the search at the root. Returning a failure
  // stops the walk immediately. Only the active composite path occupies stack
  // space, even when the descriptor represents a very large repeated object.
  template <typename Consumer>
  constexpr auto visit(Consumer consumer) const -> Ttx::Data::Status;

  // Prepared selections can ask for a sorted set of exact byte coordinates.
  // This walk skips the unrequested instances of a range arithmetically, so
  // selecting its final element skips the preceding elements.
  // Coordinates must be unique and increasing. A coordinate in padding or
  // inside a primitive returns Bounds rather than rounding to another value.
  template <typename Consumer>
  constexpr auto visit(
      Perimortem::Core::View::Vector<Count> coordinates,
      Consumer consumer) const -> Ttx::Data::Status;

 private:
  ttx_representation value;
  Perimortem::Memory::Global::Bytes bytes;
};

constexpr auto Representation::get_extent() const -> Count {
  const U8 depth = get_depth();
  return Ttx::Data::Encoding::Struct::decode(get_blocks(), 0, depth).extent;
}

constexpr auto Representation::get_alignment() const -> Count {
  const U8 depth = get_depth();
  return Ttx::Data::Encoding::Struct::decode(get_blocks(), 0, depth).alignment;
}

constexpr auto Representation::compatible(const Representation& other) const
    -> Bool {
  if (value.size != other.value.size) {
    return False;
  }

  if (value.data == other.value.data) {
    return True;
  }

  return Perimortem::Core::Data::compare(
      value.data, other.value.data, value.size);
}

template <typename Consumer>
constexpr auto Representation::visit(Consumer consumer) const
    -> Ttx::Data::Status {
  using Ttx::Data::Status;
  const U8 depth = get_depth();
  const auto bytes = get_blocks();
  // Keeping both widths explicit lets the compiler eliminate Position's
  // default width fallback without emitting a separate traversal for each.
  const bool narrow = get_pointer_size() == 4;
  const auto walk = [&](this auto&& self, Count body, Count offset) -> Status {
    const auto header = Ttx::Data::Encoding::Struct::decode(bytes, body, depth);
    for (Count i = 0; i < header.count; ++i) {
      const auto entry =
          Ttx::Data::Encoding::Element::decode(bytes, body + i + 1, depth);
      // The descriptor chooses one operation for the whole run. Keeping that
      // choice outside the loop lets primitive runs reuse their decoded facts
      // without testing for recursion at every position.
      if (entry.is_inline()) {
        for (Count repetition = 0; repetition < entry.count; ++repetition) {
          const auto status = self(
              entry.type, offset + entry.offset + repetition * entry.distance);
          if (status != Status::Success) {
            return status;
          }
        }
      } else {
        Position position(
            offset + entry.offset, entry.get_value(), entry.get_byte_order(),
            U32(entry.get_extent(narrow ? 4 : 8)));
        for (Count repetition = 0; repetition < entry.count; ++repetition) {
          const auto status = consumer(position);
          if (status != Status::Success) {
            return status;
          }

          position = Position(
              position.get_offset() + entry.distance, position.get_value(),
              position.get_byte_order(), U32(position.get_extent()));
        }
      }
    }

    return Status::Success;
  };

  return walk(0, 0);
}

template <typename Consumer>
constexpr auto Representation::visit(
    Perimortem::Core::View::Vector<Count> coordinates,
    Consumer consumer) const -> Ttx::Data::Status {
  using Ttx::Data::Status;
  const U8 depth = get_depth();
  Count selected = 0;

  const auto bytes = get_blocks();
  const bool narrow = get_pointer_size() == 4;
  const auto walk = [&](this auto&& self, Count body, Count offset) -> Status {
    const auto header = Ttx::Data::Encoding::Struct::decode(bytes, body, depth);
    for (Count i = 0; i < header.count && selected < coordinates.get_size();
         ++i) {
      const auto entry =
          Ttx::Data::Encoding::Element::decode(bytes, body + i + 1, depth);
      const Count first = offset + entry.offset;
      Count width;
      if (entry.is_inline()) {
        const auto child =
            Ttx::Data::Encoding::Struct::decode(bytes, entry.type, depth);
        width = child.extent;
      } else {
        width = entry.get_extent(narrow ? 4 : 8);
      }

      const Count end = first + (entry.count - 1) * entry.distance + width;

      // The next requested coordinate selects an instance directly. Keeping
      // the descriptor here lets later requests in this same run reuse it.
      while (selected < coordinates.get_size() && coordinates[selected] < end) {
        const Count requested = coordinates[selected];
        if (requested < first) {
          return Status::Bounds;
        }

        const Count instance = (requested - first) / entry.distance;
        const Count start = first + instance * entry.distance;
        Status status;
        if (entry.is_inline()) {
          if (requested >= start + width) {
            return Status::Bounds;
          }

          const Count before = selected;
          status = self(entry.type, start);
          if (selected == before && status == Status::Success) {
            return Status::Bounds;
          }
        } else {
          if (requested != start) {
            return Status::Bounds;
          }

          const Position position(
              start, entry.get_value(), entry.get_byte_order(),
              U32(entry.get_extent(narrow ? 4 : 8)));
          status = consumer(position);
          ++selected;
        }

        if (status != Status::Success) {
          return status;
        }
      }
    }

    return Status::Success;
  };

  const auto status = walk(0, 0);
  if (status != Status::Success) {
    return status;
  }

  return selected == coordinates.get_size() ? Status::Success : Status::Bounds;
}

}  // namespace Ttx::Data::Form

TTX_DATA_RECORD(
    ttx_representation,
    TTX_DATA_MEMBER(ttx_representation, data),
    TTX_DATA_MEMBER(ttx_representation, size));

#endif

#endif
