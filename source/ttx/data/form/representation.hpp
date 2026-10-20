// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/storage/result.hpp"
#include "perimortem/memory/global/bytes.hpp"

#include "ttx/data/encoding/element.hpp"
#include "ttx/data/encoding/struct.hpp"
#include "ttx/data/form/native.hpp"
#include "ttx/data/form/representation.h"
#include "ttx/data/status.hpp"

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
  // Navigation returns a primitive occurrence by value. Its byte coordinates
  // belong to the containing object, while type and byte order describe the
  // observation at that coordinate. No pointer to a reconstructed child object
  // needs to survive the lookup.
  // Pointer and callable slots both produce a Pointer observation whose extent
  // comes from the representation's pointer width, including when the reader
  // runs on another target. Navigation reports the pointer slot's coordinates.
  // It reads the encoded descriptions rather than dereferencing addresses in
  // the payload. Target descriptions participate in comparison of the complete
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

  // C operations observe this record and its descriptor bytes. The caller
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
  // each successive observation does not repeat this search from the root.
  EXPORTED(TTX) auto next(Count offset) const
      -> Perimortem::Core::Storage::Result<Position, Ttx::Data::Status>;

  // Whole operations consume primitive occurrences in byte order. Repeated
  // composites revisit their shared body for each instance, but successive
  // observations never restart the search at the root. Returning a failure
  // stops the walk immediately. Only the active composite path occupies stack
  // space, even when the descriptor represents a very large repeated object.
  template <typename Consumer>
  constexpr auto visit(Consumer consumer) const -> Ttx::Data::Status;

  // Prepared selections can ask for a sorted set of exact byte coordinates.
  // This walk skips the unrequested instances of a range arithmetically, so
  // selecting its final element does not observe all preceding elements.
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
