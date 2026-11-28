// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/alias.hpp"

#include "ttx/graph/capabilities/normalize.hpp"
#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;

class AliasProvider {
  // A foreign normalizer can call back through a slot. Keep the active
  // delegations for this thread until each call returns. A call through the
  // same slot to the same provider would repeat a pending resolution.
  struct Delegation {
    void* slot;
    ttx_abstract provider;
    Delegation* previous;
    static thread_local Delegation* active;

    Delegation(void* origin, ttx_abstract target)
        : slot(origin), provider(target), previous(active) {
      active = this;
    }

    ~Delegation() { active = previous; }
  };

  static auto complete(ttx_abstract target) -> Bool {
    const auto* ops = target.operations;
    return target.context && ops && ops->supports && ops->bind &&
           ops->resolve_route && ops->visit_routes && ops->get_reachability;
  }

  static auto points_to_self(void* context, ttx_abstract referent) -> Bool {
    return referent.context == context && referent.operations == &operations;
  }

  static auto resolve_provider(
      void* slot,
      ttx_abstract target,
      ttx_abstract* output) -> ttx_negotiation_status {
    for (auto* frame = Delegation::active; frame; frame = frame->previous) {
      if (frame->slot == slot &&
          Abstract(frame->provider) == Abstract(target)) {
        return TTX_NEGOTIATION_UNKNOWN;
      }
    }

    Delegation delegation(slot, target);
    ttx_normalize normalizer{};
    const auto status = target.operations->bind(
        target.context, Capabilities::Normalize::contract_id, &normalizer);
    if (status == TTX_NEGOTIATION_SATISFIED) {
      if (!normalizer.operations || !normalizer.operations->resolve) {
        return TTX_NEGOTIATION_REJECTED;
      }

      ttx_abstract result{};
      const auto resolved =
          normalizer.operations->resolve(normalizer.context, &result);
      if (resolved != TTX_NEGOTIATION_SATISFIED) {
        return resolved;
      }

      // A provider can issue different records for one semantic location.
      // An identical record already disproves a strict resolution. For other
      // records the foreign provider must establish the distinct final subject.
      if (!complete(result) || Abstract(result) == Abstract(target) ||
          points_to_self(slot, result)) {
        return TTX_NEGOTIATION_REJECTED;
      }

      *output = result;
      return TTX_NEGOTIATION_SATISFIED;
    }

    // A closed None or Unknown already denotes its final subject. Recognize
    // the policy through the supplying provider so independently implemented
    // sentinels keep their own context and operations.
    const Abstract subject(target);
    if (subject.supports<Policies::None>() == Negotiation::Status::Satisfied ||
        subject.supports<Policies::Unknown>() ==
            Negotiation::Status::Satisfied) {
      *output = target;
      return TTX_NEGOTIATION_SATISFIED;
    }

    return status;
  }

 public:
  static auto get_referent(void* context) -> ttx_abstract {
    return Data::cast<ttx_alias_slot>(context)->referent;
  }

  static auto set_referent(void* context, ttx_abstract target)
      -> ttx_negotiation_status {
    if (!complete(target)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    Data::cast<ttx_alias_slot>(context)->referent = target;
    return TTX_NEGOTIATION_SATISFIED;
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    const Identity::Uuid contract(id);
    if (contract == Capabilities::Normalize::contract_id) {
      return points_to_self(context, get_referent(context))
                 ? TTX_NEGOTIATION_REJECTED
                 : TTX_NEGOTIATION_SATISFIED;
    }

    return contract == Abstract::contract_id ||
                   contract == Policies::Alias::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    const Identity::Uuid contract(id);
    if (contract == Abstract::contract_id) {
      *Data::cast<ttx_abstract>(output) = get_abstract(context);
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (contract == Policies::Alias::contract_id) {
      *Data::cast<ttx_alias>(output) = {context, &alias_operations};
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (contract == Capabilities::Normalize::contract_id) {
      if (points_to_self(context, get_referent(context))) {
        return TTX_NEGOTIATION_REJECTED;
      }

      *Data::cast<ttx_normalize>(output) = {context, &normalize_operations};
      return TTX_NEGOTIATION_SATISFIED;
    }

    return TTX_NEGOTIATION_REJECTED;
  }

  // Brent's cycle detection doubles the distance between saved slot addresses.
  // A cyclic chain eventually revisits the saved slot, while a finite chain
  // reaches another provider after one read per link. Only slots published
  // with our own operations table have a context we can read directly.
  static auto resolve(void* context, ttx_abstract* output)
      -> ttx_negotiation_status {
    void* checkpoint = context;
    Count interval = 1;
    Count distance = 0;
    auto target = get_referent(context);
    if (points_to_self(context, target)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    while (target.operations == &operations) {
      if (target.context == checkpoint) {
        return TTX_NEGOTIATION_UNKNOWN;
      }

      ++distance;
      if (distance == interval) {
        checkpoint = target.context;
        interval *= 2;
        distance = 0;
      }

      target = get_referent(target.context);
    }

    return resolve_provider(context, target, output);
  }

  // Ordinary routes belong to the slot. Forwarding a lookup that returned
  // None would commit this slot to permanent absence on that route even if
  // the next referent supplied a child there.
  static auto route(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    return ttx_none();
  }

  static auto visit(void* context, ttx_graph_visitor visitor) -> void {}

  static auto get_reachability(void* context) -> ttx_abstract_reachability {
    return TTX_ABSTRACT_REACHABLE;
  }

  static auto get_abstract(void* context) -> ttx_abstract {
    return {context, &operations};
  }

 private:
  static constexpr ttx_alias_ops alias_operations = {
    get_referent, set_referent};

  static constexpr ttx_normalize_ops normalize_operations = {resolve};

  static constexpr ttx_abstract_ops operations = {
    supports, bind, route, visit, get_reachability, ttx_resolve_self};
};

thread_local AliasProvider::Delegation* AliasProvider::Delegation::active =
    nullptr;

auto ttx_alias_initialize(ttx_alias_slot* slot) -> ttx_abstract {
  slot->referent = ttx_unknown();
  return AliasProvider::get_abstract(slot);
}
