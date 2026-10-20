// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/data.hpp"
#include "perimortem/core/storage/vector.hpp"
#include "perimortem/core/view/bytes.hpp"
#include "perimortem/serialization/formats/uuid.hpp"

#include "ttx/concept/abstract.h"
#include "ttx/concept/visitor.hpp"
#include "ttx/semantic/negotiation/query.hpp"

namespace Ttx::Concept {

// Abstract uses the provider's context and operations to traverse a semantic
// graph. The C++ interface stores the C API record and forwards each call to
// its supplied function with the context published alongside it.
//
// Abstract-to-Abstract graph operations are observations. Binding enters a
// typed API with its complete contract. Terminal operations use known types,
// such as a Cursor returning a Token. A typed operation can return an Abstract
// anywhere in the graph, where observations resume. Each later binding is a
// separate request. The graph contract and examples are in abstract.h.
//
// The supplying contract keeps the context and table accessible through use.
// Copying an Abstract preserves that access duration. A plain Abstract result
// requires no release. An operation transferring a release obligation returns
// Retained or a stronger contract that exposes the obligation.
class Abstract {
 public:
  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_ABSTRACT_ID_HIGH,
      TTX_ABSTRACT_ID_LOW);
  using Api = ttx_abstract;
  using Operations = ttx_abstract_ops;
  using Visitor = Concept::Visitor<Abstract>;

  // Checks the context, table and required operation pointers for null. The
  // caller establishes the representation and readability of any present table.
  // The provider supplies the semantic guarantees and duration of access.
  static auto accept(Api api) -> Bool {
    return api.context && api.operations && api.operations->supports &&
           api.operations->bind && api.operations->get_data &&
           api.operations->resolve && api.operations->resolve_concept &&
           api.operations->visit_concepts;
  }

  // Stores the context pointer and the address of the operation table. The
  // caller supplies a complete Abstract interface and keeps its context and
  // table available for every use of this record.
  constexpr Abstract(void* context, const Operations& operations)
      : api(context, &operations) {}

  // Copies a record already supplied under the Abstract contract. The caller
  // establishes its representation and required pointers. Explicit construction
  // marks this unchecked conversion from the C record.
  explicit constexpr Abstract(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_query() const -> Semantic::Negotiation::Query {
    return Semantic::Negotiation::Query(ttx_semantic_query(
        api.context, api.operations->bind, api.operations->supports));
  }

  auto supports(Perimortem::Serialization::Formats::Uuid contract) const
      -> Semantic::Negotiation::Status {
    return get_query().supports(contract);
  }

  template <typename Contract>
  auto supports() const -> Semantic::Negotiation::Status {
    return supports(Contract::contract_id);
  }

  auto bind_interface(
      Perimortem::Serialization::Formats::Uuid contract,
      Data::Form::Storage destination) const -> Semantic::Negotiation::Status {
    return get_query().bind(contract, destination);
  }

  template <typename Contract>
  auto bind() const -> Perimortem::Core::Storage::
      Result<Contract, Semantic::Negotiation::Status> {
    return get_query().template bind<Contract>();
  }

  // Provides direct byte access under the already established Abstract
  // contract. This typed read can supply bytes alongside `resolve_concept`,
  // or exchange bytes for sockets and streams.
  //
  // The bytes have no Abstract contract to inspect and no implied format or
  // Constant promise. Repeated calls can read changing data even when a
  // semantic edge on the same Abstract is Constant.
  //
  // The returned view survives until the next byte read or graph observation
  // on this Abstract, or the end of the supplying lifetime. Consumers needing
  // longer retention can copy the bytes or negotiate a contract that provides
  // it.
  auto get_data() const -> Perimortem::Core::View::Bytes {
    const auto data = api.operations->get_data(api.context);
    return Perimortem::Core::View::Bytes(data);
  }

  // Returns the Abstract represented by this observation. The provider decides
  // which domain layers resolution removes and may return the current view.
  // For an unchanged observation state, resolution is idempotent:
  //
  //   abstract.resolve() == abstract.resolve().resolve()
  //
  // Equality identifies the context and operation table. Semantic equivalence
  // follows the policies supplied by the provider.
  auto resolve() const -> Abstract {
    return Abstract(api.operations->resolve(api.context));
  }

  // Traverses a route through the provider's policy. The returned Abstract
  // supports continued traversal and UUID negotiation. The provider owns the
  // binary route format. A route can be a name or instructions for negotiation
  // between domains.
  //
  // None and Unknown return themselves for further routes. Chaining resolution
  // preserves either terminal result regardless of which step reached it.
  //
  // Each call is a separate observation and may return a different result.
  // A value that provides `Constant` promises it only for the particular edge
  // established by the Abstract and route pair. The consumer can reuse that
  // selection while its supplying access remains valid. Constant preserves
  // the selection independently of the contract that determines its lifetime.
  auto resolve_concept(Perimortem::Core::View::Bytes route) const -> Abstract {
    return Abstract(api.operations->resolve_concept(
        api.context,
        perimortem_view_bytes(route.get_data(), route.get_size())));
  }

  // The provider advertises the routes visible through this Abstract's policy.
  // Callbacks let it walk its own representation without first constructing
  // and transferring a collection of API records, at the cost of a callback for
  // each advertised route.
  //
  // Enumeration order has no implied meaning, and each visit is a separate
  // observation. A consumer needing a particular selection or representation
  // can negotiate that contract directly instead of walking all routes.
  //
  // Callbacks finish before visitation returns and must not invalidate the
  // traversed state. Route bytes survive only their callback and must be copied
  // if retained. References encoded in those bytes follow the provider's route
  // contract.
  auto visit_concepts(Visitor visitor) const -> void {
    const ttx_concept_visitor callback(&visitor, callback_concept);
    api.operations->visit_concepts(api.context, callback);
  }

  constexpr auto operator==(const Abstract& other) const -> Bool {
    return api.context == other.api.context &&
           api.operations == other.api.operations;
  }

  constexpr auto operator!=(const Abstract& other) const -> Bool {
    return !(*this == other);
  }

 private:
  // `visit_concepts` supplies its local Visitor as context and keeps it alive
  // until synchronous enumeration finishes.
  static auto callback_concept(
      void* context,
      perimortem_view_bytes route,
      Api abstract) -> void {
    auto& visitor = *Perimortem::Core::Data::cast<Visitor>(context);
    visitor(Perimortem::Core::View::Bytes(route), Abstract(abstract));
  }

  Api api;
};

static_assert(sizeof(Abstract) == sizeof(ttx_abstract));

}  // namespace Ttx::Concept

TTX_DATA_RECORD(
    perimortem_view_bytes,
    TTX_DATA_MEMBER(perimortem_view_bytes, data),
    TTX_DATA_MEMBER(perimortem_view_bytes, size));

// Declare the root schema first so recursive signatures can refer back to it.
// Native reads argument and result types from the C fields and supplies their
// calling convention. The root's fields are defined after the operation and
// visitor schemas, once their references are available.
template <>
class Ttx::Data::Form::Native<ttx_abstract> {
  static_assert(
      __is_standard_layout(ttx_abstract) &&
          __is_trivially_constructible(ttx_abstract, const ttx_abstract&) &&
          __is_trivially_destructible(ttx_abstract),
      "Native API records require a trivial C-compatible call boundary");

 public:
  static const Schema schema;
  static constexpr Schema::Reference reference = schema;
};

TTX_DATA_RECORD(
    ttx_concept_visitor,
    TTX_DATA_MEMBER(ttx_concept_visitor, context),
    TTX_DATA_MEMBER(ttx_concept_visitor, callback));

TTX_DATA_RECORD(
    ttx_abstract_ops,
    TTX_DATA_MEMBER(ttx_abstract_ops, supports),
    TTX_DATA_MEMBER(ttx_abstract_ops, bind),
    TTX_DATA_MEMBER(ttx_abstract_ops, get_data),
    TTX_DATA_MEMBER(ttx_abstract_ops, resolve),
    TTX_DATA_MEMBER(ttx_abstract_ops, resolve_concept),
    TTX_DATA_MEMBER(ttx_abstract_ops, visit_concepts));

inline constexpr Ttx::Data::Form::Schema
    Ttx::Data::Form::Native<ttx_abstract>::schema = [] {
      static constexpr Perimortem::Core::Storage::Vector<Schema::Position, 2>
          fields = {{
            TTX_DATA_MEMBER(ttx_abstract, context),
            TTX_DATA_MEMBER(ttx_abstract, operations),
          }};

      return Schema::composite(
          fields.get_view(), sizeof(ttx_abstract), alignof(ttx_abstract));
    }();
