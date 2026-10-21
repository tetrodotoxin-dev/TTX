// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/data.hpp"
#include "perimortem/core/storage/option.hpp"
#include "perimortem/core/storage/vector.hpp"
#include "perimortem/core/view/bytes.hpp"
#include "perimortem/serialization/formats/uuid.hpp"

#include "ttx/graph/abstract.h"
#include "ttx/graph/visitor.hpp"
#include "ttx/semantic/negotiation/query.hpp"

namespace Ttx::Graph {

// Abstract uses the provider's context and operations to traverse a semantic
// graph. The C++ interface stores the C API record and forwards each call to
// its supplied function with the context published alongside it.
//
// Graph navigation is independent of the consumer's native type system.
// Binding supplies a typed API whose terminal operations follow that contract.
// Returning an Abstract makes graph observations available again at the
// location chosen by the provider. A contract offering `get_abstract` preserves
// the location from which it was bound. The full graph contract is in
// abstract.h.
//
// The supplying contract defines the context and table access duration.
// Copies of an Abstract share that duration. A plain Abstract result requires
// no release. An operation transferring a release obligation returns
// Retained or a stronger contract that exposes the obligation.
class Abstract {
 public:
  enum class State : U8 {
    None = TTX_ABSTRACT_NONE,
    Unknown = TTX_ABSTRACT_UNKNOWN,
    Present = TTX_ABSTRACT_PRESENT,
  };

  static constexpr auto contract_id = Perimortem::Serialization::Formats::Uuid(
      TTX_ABSTRACT_ID_HIGH,
      TTX_ABSTRACT_ID_LOW);
  using Api = ttx_abstract;
  using Operations = ttx_abstract_ops;
  using Visitor = Graph::Visitor<Abstract>;

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

  // Reports the graph result independently of any application UUID. Present
  // permits further navigation under this Abstract's policy, even when every
  // application contract requested so far was refused or undetermined.
  auto get_state() const -> State {
    return State(api.operations->get_state(api.context));
  }

  auto supports(Perimortem::Serialization::Formats::Uuid contract) const
      -> Semantic::Negotiation::Status {
    return Semantic::Negotiation::Status(
        api.operations->supports(api.context, contract));
  }

  template <typename Contract>
  auto supports() const -> Semantic::Negotiation::Status {
    return supports(Contract::contract_id);
  }

  // A successful UUID request supplies the complete Contract::Api record.
  // The provider establishes its operations and policies. The empty Option
  // represents either an undetermined or refused request.
  template <typename Contract>
  auto bind() const -> Perimortem::Core::Storage::Option<Contract> {
    return bind<Contract>(Contract::contract_id);
  }

  // The caller selects the C record specified by this UUID. This overload
  // supports contracts whose UUID is discovered while navigating the graph.
  template <typename Contract>
  auto bind(Perimortem::Serialization::Formats::Uuid contract) const
      -> Perimortem::Core::Storage::Option<Contract> {
    typename Contract::Api output{};
    const auto status = api.operations->bind(api.context, contract, &output);
    if (status == TTX_NEGOTIATION_SATISFIED) {
      return Contract(output);
    }

    return {};
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
  auto resolve_route(Perimortem::Core::View::Bytes route) const -> Abstract {
    return Abstract(api.operations->resolve_route(
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
  auto visit_routes(Visitor visitor) const -> void {
    const ttx_graph_visitor callback(&visitor, callback_route);
    api.operations->visit_routes(api.context, callback);
  }

  // Compares the published context and table pointers. The `get_abstract` round
  // trip preserves semantic graph location, which can have different API
  // records.
  constexpr auto operator==(const Abstract& other) const -> Bool {
    return api.context == other.api.context &&
           api.operations == other.api.operations;
  }

  constexpr auto operator!=(const Abstract& other) const -> Bool {
    return !(*this == other);
  }

 private:
  // `visit_routes` supplies its local Visitor as context and keeps it alive
  // until synchronous enumeration finishes.
  static auto callback_route(
      void* context,
      perimortem_view_bytes route,
      Api abstract) -> void {
    auto& visitor = *Perimortem::Core::Data::cast<Visitor>(context);
    visitor(Perimortem::Core::View::Bytes(route), Abstract(abstract));
  }

  Api api;
};

static_assert(sizeof(Abstract) == sizeof(ttx_abstract));

}  // namespace Ttx::Graph

TTX_DATA_RECORD(
    perimortem_view_bytes,
    TTX_DATA_MEMBER(perimortem_view_bytes, data),
    TTX_DATA_MEMBER(perimortem_view_bytes, size));

// Semantic bootstrap checks Abstract's complete C ABI against this description.
// Typed Graph bindings use the UUID and fixed C contract directly.
//
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
    ttx_graph_visitor,
    TTX_DATA_MEMBER(ttx_graph_visitor, callback_context),
    TTX_DATA_MEMBER(ttx_graph_visitor, callback));

TTX_DATA_RECORD(
    ttx_abstract_ops,
    TTX_DATA_MEMBER(ttx_abstract_ops, supports),
    TTX_DATA_MEMBER(ttx_abstract_ops, bind),
    TTX_DATA_MEMBER(ttx_abstract_ops, resolve_route),
    TTX_DATA_MEMBER(ttx_abstract_ops, visit_routes),
    TTX_DATA_MEMBER(ttx_abstract_ops, get_state));

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
