// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/storage/result.hpp"
#include "perimortem/serialization/formats/uuid.hpp"

#include "ttx/data/form/storage.hpp"
#include "ttx/semantic/negotiation/binding.hpp"
#include "ttx/semantic/negotiation/query.h"

namespace Ttx::Semantic::Negotiation {

// Query calls the provider through its negotiated C API record. Typed binding
// supplies storage for the requested API and returns its C++ interface after
// successful negotiation. An interface can provide an additional `accept`
// check for the returned record. Representation and semantic agreement remain
// the same as for a caller that supplies Storage directly.
//
// The record borrows its context and functions from the supplying contract.
// Each operation returns Rejected if its required function is absent.
class Query {
 public:
  constexpr Query() = default;

  // Copies the C record without calling its functions or checking its
  // representation. The caller establishes that representation first. Explicit
  // construction marks this unchecked conversion at the call site.
  constexpr explicit Query(ttx_semantic_query api) : api(api) {}

  constexpr operator ttx_semantic_query() const { return api; }

  // Reports whether both operation pointers are present. Default construction
  // leaves both absent. Pointer presence alone establishes neither their
  // calling convention nor the provider's semantic guarantees.
  constexpr auto is_set() const -> Bool {
    return api.bind != nullptr && api.supports != nullptr;
  }

  EXPORTED(TTX) auto supports(Perimortem::Serialization::Formats::Uuid contract)
      const -> Binding::Status;

  template <typename Contract>
  auto supports() const -> Binding::Status {
    return supports(Contract::contract_id);
  }

  EXPORTED(TTX) auto bind(
      Perimortem::Serialization::Formats::Uuid contract,
      Data::Form::Storage requested) const -> Binding::Status;

  template <typename Contract>
  auto bind() const
      -> Perimortem::Core::Storage::Result<Contract, Binding::Status> {
    return bind<Contract>(Contract::contract_id);
  }

  // The UUID selects the semantic contract, while Interface::Api describes
  // the requested record. A provider can support that UUID in several record
  // formats and selects the one matching the supplied Storage.
  //
  // After Satisfied, `Interface::accept(api)` runs when that operation exists.
  // A false result returns Rejected. Otherwise construction receives the
  // complete API record. The provider establishes its semantic guarantees
  // regardless of whether the C++ interface supplies this additional check.
  template <typename Interface>
  auto bind(Perimortem::Serialization::Formats::Uuid contract) const
      -> Perimortem::Core::Storage::Result<Interface, Binding::Status> {
    typename Interface::Api api = {};
    const auto requested = Data::Form::Storage::from(api);
    const auto status = bind(contract, requested);
    if (status != Binding::Status::Satisfied) {
      return status;
    }

    if constexpr (requires { Interface::accept(api); }) {
      if (!Interface::accept(api)) {
        return Binding::Status::Rejected;
      }
    }

    return Interface(api);
  }

 private:
  ttx_semantic_query api = ttx_semantic_query();
};

}  // namespace Ttx::Semantic::Negotiation

TTX_DATA_RECORD(
    perimortem_uuid,
    TTX_DATA_MEMBER(perimortem_uuid, high),
    TTX_DATA_MEMBER(perimortem_uuid, low));
TTX_DATA_RECORD(
    ttx_semantic_query,
    TTX_DATA_MEMBER(ttx_semantic_query, context),
    TTX_DATA_MEMBER(ttx_semantic_query, bind),
    TTX_DATA_MEMBER(ttx_semantic_query, supports));
