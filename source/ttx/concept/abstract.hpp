// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/bytes.hpp"
#include "perimortem/core/static/vector.hpp"
#include "perimortem/core/option.hpp"

#include "perimortem/system/uuid.hpp"

#include "ttx/abi/receiver.hpp"
#include "ttx/concept/abstract.h"
#include "ttx/concept/policies/unknown.h"
#include "ttx/concept/visitor.hpp"
#include "ttx/semantic/negotiation/query.hpp"

namespace Ttx::Concept {

// An Abstract is the entry view of a subject that a provider agreed to expose.
// The C receiver and ops table makes the same view usable across languages and
// systems, but it can be a bit cumbersome to use. This C++ API provides a more
// native API layer that also allows for optimizations inside the same module
// that the C layer can't expose.
//
// The table's complete callable form still participates in TTX's semantic
// binding. Once this record is acquired navigation can use Abstract operations
// directly, but it should avoid assuming it follows any specific C++ object
// model.
//
// Abstract is a view valid within the observation that supplied it. Copying
// the view keeps the same receiver and table. The supplying call or retained
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
    return api.source && api.operations && api.operations->supports &&
           api.operations->bind && api.operations->get_data &&
           api.operations->resolve && api.operations->resolve_concept &&
           api.operations->visit_concepts;
  }

  constexpr Abstract(void* source, const Operations& operations)
      : value(source, &operations) {}
  explicit constexpr Abstract(Api value) : value(value) {}

  constexpr auto get_abi() const -> Api { return value; }
  constexpr auto get_query() const -> Semantic::Negotiation::Query {
    return Semantic::Negotiation::Query(ttx_semantic_query(
        value.source, value.operations->bind, value.operations->supports));
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
    if constexpr (__is_same(Contract, Abstract)) {
      return *this;
    } else {
      return get_query().template bind<Contract>();
    }
  }

  // Gets this Abstract's byte observation without creating another Abstract to
  // describe it. This makes it useful as a `resolve_concept` side channel, or
  // for sockets and streams that already have bytes to exchange.
  //
  // The bytes have no Abstract contract to inspect and no implied format or
  // Constant promise. Repeated calls can observe changing data even when a
  // semantic edge on the same subject is Constant.
  //
  // The returned view survives until the next observation on this subject or
  // publication release. Consumers needing longer retention can copy the bytes
  // or negotiate a contract that provides it.
  auto get_data() const -> Perimortem::Core::View::Bytes {
    const auto data = value.operations->get_data(value.source);
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
    return Abstract(value.operations->resolve(value.source));
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
    return Abstract(value.operations->resolve_concept(
        value.source,
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
    const ttx_concept_visitor receiver = ttx_concept_visitor(
        &visitor, [](void* source, perimortem_view_bytes route, Api subject) {
          Abi::Receiver::get<Visitor>(source)(
              Perimortem::Core::View::Bytes(route.data, route.size),
              Abstract(subject));
        });
    value.operations->visit_concepts(value.source, receiver);
  }

  // This token identifies the provider within its enclosing publication. It
  // proves neither a C++ type nor identity across independent publications.
  constexpr auto get_identity() const -> const void* { return value.source; }

  constexpr auto operator==(const Abstract& other) const -> Bool {
    return value.source == other.value.source &&
           value.operations == other.value.operations;
  }
  constexpr auto operator!=(const Abstract& other) const -> Bool {
    return !(*this == other);
  }

  // Attempts to recover the native provider when this module published the
  // exact C++ provider type. Comparing the private operation tables proves the
  // relationship without negotiating another interface. A foreign publication
  // cannot supply that local proof, even when its type has the same name and
  // layout.
  //
  // Code with an independently known native object can use it directly. That
  // knowledge does not establish the C++ type of later graph observations,
  // which may return substitutes.
  //
  // Only the matching local table grants the native reference. A ReadOnly
  // projection uses another table and cannot be recovered through this path.
  template <typename Provider>
  auto cast() const -> Perimortem::Core::Option<Provider&> {
    if constexpr (__is_base_of(Abstract, Provider)) {
      return Perimortem::Core::Option<Provider&>();
    } else {
      Bool native = value.operations == &provider_operations<Provider>();
      if (!native) {
        return Perimortem::Core::Option<Provider&>();
      }

      return Abi::Receiver::get<Provider>(value.source);
    }
  }

  // Adapts a native provider to the Abstract interface. An object already
  // carrying an Abstract view supplies that existing view, preserving the
  // provider and policy it represents. Deriving a C++ facade from Abstract
  // therefore doesn't publish the facade as a new graph subject.
  //
  // Other native providers are published through generated thunks. Those thunks
  // know the concrete provider type, so its implementation doesn't need to
  // inherit an Abstract base or expose its private representation.
  //
  // Native publication borrows a mutable provider. A const handle does not
  // narrow its hooks. Policies such as ReadOnly select the access they expose
  // through their own operation tables.
  template <typename Provider>
    requires(!__is_const(Provider))
  static constexpr auto provide(Provider& provider) -> Abstract {
    if constexpr (__is_base_of(Abstract, Provider)) {
      return provider;
    } else {
      return Abstract(&provider, provider_operations<Provider>());
    }
  }

 private:
  // Hidden linkage keeps table identity local to one linked unit. Otherwise
  // symbol preemption could accidentally turn a foreign publication into a
  // native cast proof when two modules instantiate the same C++ provider type.
  template <typename Provider>
  __attribute__((visibility("hidden"))) static constexpr auto
      provider_operations() -> const Operations& {
    static constexpr Operations operations = Operations(
        [](void* source, perimortem_uuid id) -> ttx_binding_status {
          const Perimortem::System::Uuid contract(id);
          if (contract == contract_id) {
            return TTX_BINDING_SATISFIED;
          }

          if constexpr (requires(Provider& provider) {
                          provider.supports(contract);
                        }) {
            return static_cast<ttx_binding_status>(
                Abi::Receiver::get<Provider>(source).supports(contract));
          }

          return TTX_BINDING_UNKNOWN;
        },
        [](void* source, perimortem_uuid id,
           ttx_storage requested) -> ttx_binding_status {
          const Data::Form::Storage target(requested);
          if (Perimortem::System::Uuid(id) == contract_id) {
            return static_cast<ttx_binding_status>(
                Semantic::Negotiation::Binding::provide<Abstract>(
                    Api(source, &operations), target));
          }

          if constexpr (requires(Provider& provider) {
                          provider.bind_interface(
                              Perimortem::System::Uuid(id), target);
                        }) {
            return static_cast<ttx_binding_status>(
                Abi::Receiver::get<Provider>(source).bind_interface(
                    Perimortem::System::Uuid(id), target));
          }

          return TTX_BINDING_UNKNOWN;
        },
        [](void* source) -> perimortem_view_bytes {
          const auto data = Abi::Receiver::get<Provider>(source).get_data();
          return perimortem_view_bytes(data.get_data(), data.get_size());
        },
        [](void* source) -> Api {
          if constexpr (requires(Provider& provider) { provider.resolve(); }) {
            return Abi::Receiver::get<Provider>(source).resolve().get_abi();
          }

          return Api(source, &operations);
        },
        [](void* source, perimortem_view_bytes route) -> Api {
          if constexpr (requires(Provider& provider) {
                          provider.resolve_concept(
                              Perimortem::Core::View::Bytes());
                        }) {
            return Abi::Receiver::get<Provider>(source)
                .resolve_concept(
                    Perimortem::Core::View::Bytes(route.data, route.size))
                .get_abi();
          }

          return ttx_unknown();
        },
        [](void* source, ttx_concept_visitor visitor) {
          if constexpr (requires(Provider& provider, Visitor visitor) {
                          provider.visit_concepts(visitor);
                        }) {
            auto receive = [&](Perimortem::Core::View::Bytes route,
                               Abstract subject) {
              visitor.receive(
                  visitor.source,
                  perimortem_view_bytes(route.get_data(), route.get_size()),
                  subject.get_abi());
            };
            Abi::Receiver::get<Provider>(source).visit_concepts(Visitor(receive));
          }
        });
    return operations;
  }

  Api value;
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
    Schema receive;
    Schema resolve;
    Schema lookup;
    Schema visit;
    Perimortem::Core::Static::Vector<Schema::Argument, 3> receive_arguments;
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
        : receive_arguments({
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
            Schema::Position(Schema::pointer(), offsetof(ttx_abstract, source)),
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
                offsetof(ttx_concept_visitor, source)),
            {
              receive,
              offsetof(ttx_concept_visitor, receive),
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
      receive = Schema::callable(
          Schema::Convention::Native, receive_arguments.get_view());
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
    TTX_DATA_MEMBER(ttx_concept_visitor, source),
    TTX_DATA_MEMBER(ttx_concept_visitor, receive));
