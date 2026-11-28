// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/data.hpp"
#include "perimortem/core/identity/uuid.hpp"
#include "perimortem/core/storage/option.hpp"
#include "perimortem/core/storage/vector.hpp"
#include "perimortem/core/view/bytes.hpp"

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
// location chosen by the provider. A typed API's `get_abstract` returns the
// location that granted that API. The full graph contract is in abstract.h.
//
// The supplying contract defines the context and table access duration.
// Copies of an Abstract share that duration. A plain Abstract result requires
// no release. Stateful resources use typed contracts that define their own
// access and release obligations.
class Abstract {
 public:
  enum class Reachability : U8 {
    Unreachable = TTX_ABSTRACT_UNREACHABLE,
    Indeterminate = TTX_ABSTRACT_INDETERMINATE,
    Reachable = TTX_ABSTRACT_REACHABLE,
  };

  static constexpr auto contract_id = Perimortem::Core::Identity::Uuid(
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

  // The C and C++ handles copy the same context and operation table in either
  // direction. The caller supplies a complete record under the Abstract
  // contract. Both copies share the original access duration.
  constexpr Abstract(Api api) : api(api) {}
  constexpr operator Api() const { return api; }

  constexpr auto get_abi() const -> Api { return api; }

  // Observes whether the selected target can be reached. The consumer can
  // ask before it knows any application contracts. A provider reporting
  // Indeterminate can still offer contracts explaining the pending result.
  auto get_reachability() const -> Reachability {
    return Reachability(api.operations->get_reachability(api.context));
  }

  auto supports(Perimortem::Core::Identity::Uuid contract) const
      -> Semantic::Negotiation::Status {
    return Semantic::Negotiation::Status(
        api.operations->supports(api.context, contract));
  }

  template <typename Contract>
  auto supports() const -> Semantic::Negotiation::Status {
    return supports(Contract::contract_id);
  }

  // A successful UUID request supplies the complete Contract::Api record.
  // Binding Abstract returns this node's own semantic location on success.
  // The empty Option represents either an undetermined or refused request.
  template <typename Contract>
  auto bind() const -> Perimortem::Core::Storage::Option<Contract> {
    typename Contract::Api output{};
    const auto status =
        api.operations->bind(api.context, Contract::contract_id, &output);
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
  // Each lookup observes the provider again. If the result supplies Constant,
  // the caller can reuse the selection for this Abstract and route during
  // the supplying access. None reports absence for this lookup.
  // A fresh lookup requires valid access to the same semantic origin.
  auto resolve_route(Perimortem::Core::View::Bytes route) const -> Abstract {
    return api.operations->resolve_route(
        api.context, perimortem_view_bytes(route.get_data(), route.get_size()));
  }

  // The provider advertises the routes visible through this Abstract's policy.
  // Callbacks let it walk its own representation without first constructing
  // and transferring a collection of API records, at the cost of a callback for
  // each advertised route.
  //
  // Each callback is an independent observation. It can make further graph
  // observations through the supplied Abstract, and those operations can
  // change provider state before the next callback. A consumer requiring one
  // transaction across several results can bind Selection when the provider
  // supplies it.
  //
  // Route bytes survive only their callback and must be copied if kept.
  // References encoded in those bytes follow the provider's route contract.
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
  // `visit_routes` supplies its local Visitor as callback context for the
  // complete synchronous enumeration.
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
    TTX_DATA_MEMBER(ttx_abstract_ops, get_reachability));

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
