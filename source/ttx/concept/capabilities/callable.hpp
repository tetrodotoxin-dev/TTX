// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/capabilities/callable.h"
#include "ttx/data/form/representation.hpp"

namespace Ttx::Concept::Capabilities {

// Exposes the capability to describe an operation that another provider can
// supply. `describe` returns a Result containing the operation's UUID, its API
// Representation and its input and output frames. The caller selects an API it
// can use, checks that API's Representation against the description, and binds
// the UUID with its required Representation before calling the operation.
//
// Each frame describes its storage and lists fields in argument order. A
// field's Abstract supplies its meaning and its offset locates the value in
// the frame. This lets values sharing a storage format expose different
// contracts.
//
// Description and Frame copy their records while borrowing the referenced
// Representations, field arrays and Abstracts from the supplying observation.
// A consumer keeping those observations copies their data or negotiates the
// retention it needs. An Unknown failure leaves the description undetermined.
// Rejected explicitly refuses it.
class Callable {
 public:
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_CALLABLE_ID_HIGH,
      TTX_CALLABLE_ID_LOW);
  using Api = ttx_callable;
  using Operations = ttx_callable_operations;

  // Checks `describe`, context and the required Abstract operations for null.
  // Any present table must be readable and match the established API
  // representation. The supplying contract establishes Callable's meaning.
  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->describe &&
           Abstract::accept(
               ttx_abstract(api.context, &api.operations->abstract));
  }

  // Copies a record already supplied under the Callable contract. The caller
  // establishes its representation and required pointers. Explicit construction
  // marks this unchecked conversion from the C record.
  explicit constexpr Callable(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract {
    return Abstract(api.context, api.operations->abstract);
  }

  // Frame exposes argument or result storage and its ordered field meanings.
  // Its Representation remains readable and canonical through use, and fields
  // contains get_size() accessible entries. Indexed access reads an entry
  // directly and requires an index below get_size().
  class Frame {
   public:
    // The caller supplies a valid description and field array before creating
    // a Frame. This constructor copies the record without checking that
    // storage, so conversion from the C record is explicit.
    explicit constexpr Frame(ttx_callable_frame frame) : frame(frame) {}

    auto get_representation() const -> Data::Form::Representation {
      return *frame.representation;
    }

    auto get_size() const -> Count { return frame.count; }

    auto get_abstract(Count index) const -> Abstract {
      return Abstract(frame.fields[index].abstract);
    }

    auto get_offset(Count index) const -> Count {
      return frame.fields[index].offset;
    }

   private:
    ttx_callable_frame frame;
  };

  // Description supplies the UUID and API form for negotiation, then the
  // payload forms and field Abstracts for preparing a call. Its referenced
  // descriptions and fields remain accessible under the supplying contract.
  // Binding the operation's UUID and API form establishes the callable record.
  class Description {
   public:
    // The caller ensures the canonical forms, field arrays and Abstracts are
    // usable before creating a Description. This constructor copies the record
    // without checking them, so conversion from the C record is explicit.
    explicit constexpr Description(ttx_callable_description description)
        : description(description) {}

    auto get_contract() const -> Perimortem::Serialization::Formats::Uuid {
      return Perimortem::Serialization::Formats::Uuid(description.contract);
    }

    auto get_representation() const -> Data::Form::Representation {
      return *description.api;
    }

    auto get_inputs() const -> Frame { return Frame(description.inputs); }

    auto get_outputs() const -> Frame { return Frame(description.outputs); }

   private:
    ttx_callable_description description;
  };

  // Returns a Description when the provider reports Satisfied and supplies a
  // nonnull API description pointer. Unknown is preserved. Every other status
  // or a missing API pointer returns Rejected. The provider establishes valid
  // canonical forms, field arrays and Abstracts. This operation checks the
  // status and API pointer, leaving those contents and ranges to the provider.
  auto describe() const -> Perimortem::Core::Storage::
      Result<Description, Semantic::Negotiation::Status> {
    const auto api = get_abi();
    ttx_callable_description output = ttx_callable_description();
    const auto status = api.operations->describe(api.context, &output);
    if (status == TTX_NEGOTIATION_SATISFIED && output.api) {
      return Description(output);
    }

    if (status == TTX_NEGOTIATION_UNKNOWN) {
      return Semantic::Negotiation::Status::Unknown;
    }

    return Semantic::Negotiation::Status::Rejected;
  }

 private:
  Api api;
};

}  // namespace Ttx::Concept::Capabilities

TTX_DATA_RECORD(
    ttx_callable_field,
    TTX_DATA_MEMBER(ttx_callable_field, abstract),
    TTX_DATA_MEMBER(ttx_callable_field, offset));

TTX_DATA_RECORD(
    ttx_callable_frame,
    TTX_DATA_MEMBER(ttx_callable_frame, representation),
    TTX_DATA_MEMBER(ttx_callable_frame, fields),
    TTX_DATA_MEMBER(ttx_callable_frame, count));

TTX_DATA_RECORD(
    ttx_callable_description,
    TTX_DATA_MEMBER(ttx_callable_description, contract),
    TTX_DATA_MEMBER(ttx_callable_description, api),
    TTX_DATA_MEMBER(ttx_callable_description, inputs),
    TTX_DATA_MEMBER(ttx_callable_description, outputs));

TTX_DATA_RECORD(
    ttx_callable_operations,
    TTX_DATA_MEMBER(ttx_callable_operations, abstract),
    TTX_DATA_MEMBER(ttx_callable_operations, describe));

TTX_DATA_RECORD(
    ttx_callable,
    TTX_DATA_MEMBER(ttx_callable, context),
    TTX_DATA_MEMBER(ttx_callable, operations));
