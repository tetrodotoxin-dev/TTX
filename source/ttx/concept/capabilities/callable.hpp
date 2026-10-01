// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/abi/operations.hpp"

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/capabilities/callable.h"
#include "ttx/data/form/representation.hpp"

namespace Ttx::Concept::Capabilities {

// Exposes the capability to describe an operation that another object can
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
class Callable : public Abstract {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_CALLABLE_ID_HIGH, TTX_CALLABLE_ID_LOW);
  using Api = ttx_callable;
  using Operations = ttx_callable_operations;

  static auto accept(Api api) -> Bool {
    return api.operations && api.operations->describe &&
           Abstract::accept(
               ttx_abstract(api.source, &api.operations->abstract));
  }
  explicit constexpr Callable(Api api)
      : Abstract(api.source, api.operations->abstract) {}
  constexpr auto get_abi() const -> Api {
    const auto value = Abstract::get_abi();
    return Api(
        value.source, &Abi::Operations::from_abstract<Operations>(*value.operations));
  }

  // Reads fields in argument order. Each field supplies its Abstract and byte
  // offset so the caller can interpret values and locate them in the frame.
  class Frame {
   public:
    explicit constexpr Frame(ttx_callable_frame value) : value(value) {}

    auto get_representation() const -> const Data::Form::Representation& {
      return *value.representation;
    }

    auto get_size() const -> Count { return value.count; }
    auto get_subject(Count index) const -> Abstract {
      return Abstract(value.fields[index].subject);
    }

    auto get_offset(Count index) const -> Count {
      return value.fields[index].offset;
    }

   private:
    ttx_callable_frame value;
  };

  // Holds the copied UUID and frame records. Their pointers refer to the
  // provider's representations and fields for the supplying observation.
  class Description {
   public:
    explicit constexpr Description(ttx_callable_description value)
        : value(value) {}

    auto get_contract() const -> Perimortem::System::Uuid {
      return Perimortem::System::Uuid(value.contract);
    }

    auto get_representation() const -> const Data::Form::Representation& {
      return *value.api;
    }

    auto get_inputs() const -> Frame { return Frame(value.inputs); }
    auto get_outputs() const -> Frame { return Frame(value.outputs); }

   private:
    ttx_callable_description value;
  };

  // Adapts the provider's output to a Result. A successful response supplies
  // the API Representation required to use its description. A response that
  // omits that Representation becomes a Rejected failure.
  auto describe() const -> Perimortem::Utility::
      Result<Description, Semantic::Negotiation::Binding::Failure> {
    const auto api = get_abi();
    ttx_callable_description output = ttx_callable_description();
    const auto status = api.operations->describe(api.source, &output);
    if (status == TTX_BINDING_SATISFIED && output.api) {
      return Description(output);
    }

    if (status == TTX_BINDING_UNKNOWN) {
      return Semantic::Negotiation::Binding::Failure::Unknown;
    }

    return Semantic::Negotiation::Binding::Failure::Rejected;
  }
};

}  // namespace Ttx::Concept::Capabilities

TTX_DATA_RECORD(
    ttx_callable_field,
    TTX_DATA_MEMBER(ttx_callable_field, subject),
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
    TTX_DATA_MEMBER(ttx_callable, source),
    TTX_DATA_MEMBER(ttx_callable, operations));
