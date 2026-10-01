// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.hpp"

namespace Ttx::Concept {

// Projection lets a provider expose its existing state under a set of policies.
// Hooks take the projected view before their usual arguments, so the provider
// can use `includes<Policy>` to choose the available operations. The provider
// supplies each operation according to every policy in that view.
//
// The separate hook set keeps ordinary provider methods behind the policy
// boundary. An omitted hook leaves its data or enumeration empty, or its
// question Unknown. Resolution returns the projected view itself, preserving
// that boundary for further questions.
//
// Each policy here binds through ttx_abstract. Contracts with additional
// operations, such as Borrowed, keep their complete API and caller obligations
// when the provider supplies them through a scoped hook.
template <typename... Policies>
class Projection : public Abstract {
 public:
  using Abstract::Abstract;
  template <typename Policy>
  static constexpr bool includes = (__is_same(Policy, Policies) || ...);

  // The provider admits an additional policy by composing it with the existing
  // restrictions. It supplies the same native subject so all policies continue
  // to govern that subject. An existing policy keeps the current view.
  template <typename Policy, typename Provider>
  auto with(Provider& provider) const {
    if constexpr (includes<Policy>) {
      return *this;
    } else {
      return Projection<Policies..., Policy>::provide(provider);
    }
  }

  // The caller keeps the provider alive for every use of this borrowed view.
  // Retaining access follows the provider's separately negotiated lifetime
  // policy.
  template <typename Provider>
    requires(!__is_const(Provider))
  static auto provide(Provider& provider) -> Projection {
    return Projection(ttx_abstract(&provider, &operations<Provider>()));
  }

  // Providers use this table as the Abstract prefix of a complete capability
  // API so subsequent questions continue through the scoped hooks. The provider
  // supplies the capability's operations under the same policies and preserves
  // them in further views of this subject.
  template <typename Provider>
  __attribute__((visibility("hidden"))) static auto operations()
      -> const ttx_abstract_ops& {
    static_assert(sizeof...(Policies) > 0);
    static_assert((__is_same(typename Policies::Api, ttx_abstract) && ...));
    static const ttx_abstract_ops table{
      [](void* source, perimortem_uuid id) -> ttx_binding_status {
        const Perimortem::System::Uuid contract(id);
        if (((contract == Policies::contract_id) || ...) ||
            contract == Abstract::contract_id) {
          return TTX_BINDING_SATISFIED;
        }
        const Projection view(ttx_abstract(source, &table));
        if constexpr (requires(Provider& provider) {
                        provider.supports(view, contract);
                      }) {
          return static_cast<ttx_binding_status>(
              Abi::Receiver::get<Provider>(source).supports(view, contract));
        }
        return TTX_BINDING_UNKNOWN;
      },
      [](void* source, perimortem_uuid id,
         ttx_storage requested) -> ttx_binding_status {
        const Perimortem::System::Uuid contract(id);
        const Data::Form::Storage target(requested);
        if (((contract == Policies::contract_id) || ...) ||
            contract == Abstract::contract_id) {
          return static_cast<ttx_binding_status>(
              Semantic::Negotiation::Binding::provide<Abstract>(
                  {source, &table}, target));
        }
        const Projection view(ttx_abstract(source, &table));
        if constexpr (requires(Provider& provider) {
                        provider.bind_interface(view, contract, target);
                      }) {
          return static_cast<ttx_binding_status>(
              Abi::Receiver::get<Provider>(source).bind_interface(
                  view, contract, target));
        }
        return TTX_BINDING_UNKNOWN;
      },
      [](void* source) -> perimortem_view_bytes {
        const Projection view(ttx_abstract(source, &table));
        if constexpr (requires(Provider& provider) {
                        provider.get_data(view);
                      }) {
          auto data = Abi::Receiver::get<Provider>(source).get_data(view);
          return {data.get_data(), data.get_size()};
        }
        return {};
      },
      [](void* source) -> ttx_abstract { return {source, &table}; },
      [](void* source, perimortem_view_bytes route) -> ttx_abstract {
        const Projection view(ttx_abstract(source, &table));
        const Perimortem::Core::View::Bytes name(route.data, route.size);
        if constexpr (requires(Provider& provider) {
                        provider.resolve_concept(view, name);
                      }) {
          return Abi::Receiver::get<Provider>(source)
              .resolve_concept(view, name)
              .get_abi();
        }
        return ttx_unknown();
      },
      [](void* source, ttx_concept_visitor visitor) {
        const Projection view(ttx_abstract(source, &table));
        if constexpr (
            requires(Provider& provider, Abstract::Visitor receive) {
              provider.visit_concepts(view, receive);
            }) {
          auto receive = [&](Perimortem::Core::View::Bytes route,
                             Abstract subject) {
            visitor.receive(
                visitor.source, {route.get_data(), route.get_size()},
                subject.get_abi());
          };
          Abi::Receiver::get<Provider>(source).visit_concepts(
              view, Abstract::Visitor(receive));
        }
      }};
    return table;
  }
};
}  // namespace Ttx::Concept
