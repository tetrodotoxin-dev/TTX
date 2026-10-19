// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/storage/result.hpp"
#include "perimortem/serialization/formats/uuid.hpp"

#include "ttx/data/form/storage.hpp"
#include "ttx/semantic/negotiation/binding.hpp"
#include "ttx/semantic/negotiation/query.h"

namespace Ttx::Semantic::Negotiation {

// Query keeps the supplied negotiation record. A typed request constructs
// the consumer's API storage and asks for that exact representation. The
// result becomes usable after binding and the interface's acceptance checks.
class Query {
 public:
  constexpr Query() = default;

  constexpr explicit Query(ttx_semantic_query api) : api(api) {}

  constexpr operator ttx_semantic_query() const { return api; }

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

  // A Data API describes the requested callable record independently of the
  // semantic promise identified by its UUID. Keeping both inputs explicit
  // lets that API perform admission while preserving the requested semantic
  // contract.
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
