// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/bytes.hpp"
#include "perimortem/core/static/vector.hpp"

#include "perimortem/system/uuid.hpp"

#include "ttx/concept/abstract.h"
#include "ttx/concept/visitor.hpp"
#include "ttx/semantic/negotiation/query.hpp"

namespace Ttx::Concept {

// An Abstract carries the context and operations supplied by its provider.
// Each operation interprets that context according to its own implementation.
// The C++ view copies this record and forwards calls through its functions.
//
// Abstract is a view valid within the observation that supplied it. Copying
// the view keeps the same context and table. The supplying call or retained
// provider determines when that access ends, and a later observation asks the
// provider again. Stronger return contracts state how to retain their answers.
//
// A plain Abstract return requires no release call. Returning an acquired
// answer through this weaker type hides its cleanup obligation and can leak
// resources. Providers must expose required lifetime policies in the declared
// return contract. Binding another Concept selects its operations while
// preserving the policy that governs further questions.
class Abstract {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW);
  using Api = ttx_abstract;
  using Operations = ttx_abstract_ops;
  using Visitor = Concept::Visitor<Abstract>;

  static auto accept(Api api) -> Bool {
    return api.context && api.operations && api.operations->supports &&
           api.operations->bind && api.operations->get_data &&
           api.operations->resolve && api.operations->resolve_concept &&
           api.operations->visit_concepts;
  }

  constexpr Abstract(void* context, const Operations& operations)
      : api(context, &operations) {}

  explicit constexpr Abstract(Api api) : api(api) {}

  constexpr auto get_abi() const -> Api { return api; }

  constexpr auto get_query() const -> Semantic::Negotiation::Query {
    return Semantic::Negotiation::Query(ttx_semantic_query(
        api.context, api.operations->bind, api.operations->supports));
  }

  auto supports(Perimortem::System::Uuid contract) const
      -> Semantic::Negotiation::Binding::Status {
    return get_query().supports(contract);
  }

  template <typename Contract>
  auto supports() const -> Semantic::Negotiation::Binding::Status {
    return supports(Contract::contract_id);
  }

  auto bind_interface(
      Perimortem::System::Uuid contract,
      Data::Form::Storage destination) const
      -> Semantic::Negotiation::Binding::Status {
    return get_query().bind(contract, destination);
  }

  template <typename Contract>
  auto bind() const -> Perimortem::Utility::
      Result<Contract, Semantic::Negotiation::Binding::Failure> {
    return get_query().template bind<Contract>();
  }

  // Gets this Abstract's byte observation without creating another Abstract to
  // describe it. This makes it useful as a `resolve_concept` side channel, or
  // for sockets and streams that already have bytes to exchange.
  //
  // The bytes have no Abstract contract to inspect and no implied format or
  // Constant promise. Repeated calls can observe changing data even when a
  // semantic edge on the same Abstract is Constant.
  //
  // The returned view survives until the next observation on this Abstract or
  // the end of the supplying lifetime. Consumers needing longer retention can
  // copy the bytes or negotiate a contract that provides it.
  auto get_data() const -> Perimortem::Core::View::Bytes {
    const auto data = api.operations->get_data(api.context);
    return Perimortem::Core::View::Bytes(data.data, data.size);
  }

  // Returns the Abstract represented by this observation. The provider decides
  // which domain layers resolution removes and may return the current view.
  // For an unchanged observation state, resolution is idempotent:
  //
  //   abstract.resolve() == abstract.resolve().resolve()
  //
  // Equality here identifies the same borrowed view, not equivalent semantic
  // content or an explicit shared C++ value.
  auto resolve() const -> Abstract {
    return Abstract(api.operations->resolve(api.context));
  }

  // Resolves a binary query through this Abstract's policy. The provider owns
  // the route format, so names are only one possible spelling. The bytes can
  // also encode instructions for negotiation between domains.
  //
  // Each call is a separate observation and may return a different answer.
  // A value that provides `Constant` promises it only for the particular edge
  // established by the provider + route pair, allowing the consumer to retain
  // its answer without asking again.
  auto resolve_concept(Perimortem::Core::View::Bytes route) const -> Abstract {
    return Abstract(api.operations->resolve_concept(
        api.context,
        perimortem_view_bytes(route.get_data(), route.get_size())));
  }

  // The provider advertises the routes visible through this Abstract's policy.
  // Callbacks let it walk its own representation without first constructing
  // and transferring a collection of answers, at the cost of a callback for
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

  // This context identifies the Abstract within the supplying lifetime. It
  // proves neither a C++ type nor identity across independent lifetimes.
  constexpr auto get_identity() const -> const void* { return api.context; }

  constexpr auto operator==(const Abstract& other) const -> Bool {
    return api.context == other.api.context &&
           api.operations == other.api.operations;
  }

  constexpr auto operator!=(const Abstract& other) const -> Bool {
    return !(*this == other);
  }

 private:
  static void callback_concept(
      void* context,
      perimortem_view_bytes route,
      Api abstract) {
    auto& visitor = *static_cast<Visitor*>(context);
    visitor(
        Perimortem::Core::View::Bytes(route.data, route.size),
        Abstract(abstract));
  }

  Api api;
};

static_assert(sizeof(Abstract) == sizeof(ttx_abstract));

}  // namespace Ttx::Concept

TTX_DATA_RECORD(
    perimortem_view_bytes,
    TTX_DATA_MEMBER(perimortem_view_bytes, data),
    TTX_DATA_MEMBER(perimortem_view_bytes, size));

// Abstract navigation returns Abstracts and passes them to its visitor. These
// are recursive native declarations, so their Schema nodes share one constant
// Definition instead of expanding the recursion through C++ template
// instantiation.
template <>
class Ttx::Data::Form::Native<ttx_abstract> {
  struct Definition {
    Schema root;
    Schema operations;
    Schema visitor;
    Schema callback;
    Schema resolve;
    Schema lookup;
    Schema visit;
    Perimortem::Core::Static::Vector<Schema::Argument, 3> callback_arguments;
    Perimortem::Core::Static::Vector<Schema::Argument, 1> resolve_arguments;
    Perimortem::Core::Static::Vector<Schema::Argument, 2> lookup_arguments;
    Perimortem::Core::Static::Vector<Schema::Argument, 2> visit_arguments;
    Perimortem::Core::Static::Vector<Schema::Position, 2> root_fields;
    Perimortem::Core::Static::Vector<Schema::Position, 6> operation_fields;
    Perimortem::Core::Static::Vector<Schema::Position, 2> visitor_fields;

    // The lists retain schema addresses while construction is in progress.
    // Populate each shape after the vectors exist so every view borrows live
    // storage, including the edges that point back to the root.
    constexpr Definition()
        : callback_arguments({
            Schema::pointer(),
            Native<perimortem_view_bytes>::reference,
            Schema::Argument(root),
          }),
          resolve_arguments({
            Schema::pointer(),
          }),
          lookup_arguments({
            Schema::pointer(),
            Native<perimortem_view_bytes>::reference,
          }),
          visit_arguments({
            Schema::pointer(),
            Schema::Argument(visitor),
          }),
          root_fields({
            Schema::Position(
                Schema::pointer(),
                offsetof(ttx_abstract, context)),
            {
              Schema::pointer(&operations),
              offsetof(ttx_abstract, operations),
            },
          }),
          operation_fields({
            TTX_DATA_MEMBER(ttx_abstract_ops, supports),
            TTX_DATA_MEMBER(ttx_abstract_ops, bind),
            TTX_DATA_MEMBER(ttx_abstract_ops, get_data),
            {
              resolve,
              offsetof(ttx_abstract_ops, resolve),
            },
            {
              lookup,
              offsetof(ttx_abstract_ops, resolve_concept),
            },
            {
              visit,
              offsetof(ttx_abstract_ops, visit_concepts),
            },
          }),
          visitor_fields({
            Schema::Position(
                Schema::pointer(),
                offsetof(ttx_concept_visitor, context)),
            {
              callback,
              offsetof(ttx_concept_visitor, callback),
            },
          }) {
      root = Schema::composite(
          root_fields.get_view(), sizeof(ttx_abstract), alignof(ttx_abstract));
      operations = Schema::composite(
          operation_fields.get_view(), sizeof(ttx_abstract_ops),
          alignof(ttx_abstract_ops));
      visitor = Schema::composite(
          visitor_fields.get_view(), sizeof(ttx_concept_visitor),
          alignof(ttx_concept_visitor));
      callback = Schema::callable(
          Schema::Convention::Native, callback_arguments.get_view());
      resolve = Schema::callable(
          Schema::Convention::Native, resolve_arguments.get_view(), root);
      lookup = Schema::callable(
          Schema::Convention::Native, lookup_arguments.get_view(), root);
      visit = Schema::callable(
          Schema::Convention::Native, visit_arguments.get_view());
    }
  };

  static const Definition definition;

 public:
  static constexpr const Schema& schema = definition.root;
  static constexpr const Schema& operations = definition.operations;
  static constexpr Schema::Reference reference = schema;
};

inline constexpr Ttx::Data::Form::Native<ttx_abstract>::Definition
    Ttx::Data::Form::Native<ttx_abstract>::definition;

template <>
class Ttx::Data::Form::Native<ttx_abstract_ops> {
 public:
  static constexpr const Schema& schema = Native<ttx_abstract>::operations;
  static constexpr Schema::Reference reference = schema;
};

TTX_DATA_RECORD(
    ttx_concept_visitor,
    TTX_DATA_MEMBER(ttx_concept_visitor, context),
    TTX_DATA_MEMBER(ttx_concept_visitor, callback));
