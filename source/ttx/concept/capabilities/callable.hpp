// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/capabilities/callable.h"
#include "ttx/data/form/representation.hpp"

namespace Ttx::Concept::Capabilities {

// Exposes the capability to describe an operation that another provider can
// supply. `describe` returns a Result containing the operation's UUID, its API
// Representation and its input and output frames. The caller uses that UUID
// and Representation to bind the operation on the object it wants to call.
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
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_CALLABLE_ID_HIGH, TTX_CALLABLE_ID_LOW);
  using Api = ttx_callable;
  using Operations = ttx_callable_operations;

  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->describe &&
           Abstract::accept(
               ttx_abstract(api.context, &api.operations->abstract));
  }

  explicit constexpr Callable(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_abstract() const -> Abstract {
    return Abstract(api.context, api.operations->abstract);
  }

  // Reads fields in argument order. Each field supplies its Abstract and byte
  // offset so the caller can interpret values and locate them in the frame.
  class Frame {
   public:
    explicit constexpr Frame(ttx_callable_frame frame) : frame(frame) {}

    auto get_representation() const -> const Data::Form::Representation& {
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

  // Holds the copied UUID and frame records. Their pointers refer to the
  // provider's representations and fields for the supplying observation.
  class Description {
   public:
    explicit constexpr Description(ttx_callable_description description)
        : description(description) {}

    auto get_contract() const -> Perimortem::System::Uuid {
      return Perimortem::System::Uuid(description.contract);
    }

    auto get_representation() const -> const Data::Form::Representation& {
      return *description.api;
    }

    auto get_inputs() const -> Frame { return Frame(description.inputs); }

    auto get_outputs() const -> Frame { return Frame(description.outputs); }

   private:
    ttx_callable_description description;
  };

  // Adapts the provider's output to a Result. A successful response supplies
  // the API Representation required to use its description. A response that
  // omits that Representation becomes a Rejected failure.
  auto describe() const -> Perimortem::Utility::
      Result<Description, Semantic::Negotiation::Binding::Failure> {
    const auto api = get_abi();
    ttx_callable_description output = ttx_callable_description();
    const auto status = api.operations->describe(api.context, &output);
    if (status == TTX_BINDING_SATISFIED && output.api) {
      return Description(output);
    }

    if (status == TTX_BINDING_UNKNOWN) {
      return Semantic::Negotiation::Binding::Failure::Unknown;
    }

    return Semantic::Negotiation::Binding::Failure::Rejected;
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
