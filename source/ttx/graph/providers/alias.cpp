// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/providers/alias.hpp"

#include "ttx/graph/policies/alias.h"
#include "ttx/graph/providers/none.hpp"
#include "ttx/graph/providers/resolution.hpp"
#include "ttx/graph/providers/unknown.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;

static auto get_referent(void* context) -> ttx_abstract {
  return *Data::cast<ttx_abstract>(context);
}

static auto set_referent(void* context, ttx_abstract target)
    -> Negotiation::Status {
  if (!Providers::Resolution::complete(target)) {
    return Negotiation::Status::Rejected;
  }

  *Data::cast<ttx_abstract>(context) = target;
  return Negotiation::Status::Satisfied;
}

static auto supports(void*, perimortem_uuid id) -> Negotiation::Status {
  const Identity::Uuid contract(id);
  return contract == Abstract::contract_id ||
                 contract == Policies::Alias::contract_id
             ? Negotiation::Status::Satisfied
             : Negotiation::Status::Rejected;
}

static auto bind(void* context, perimortem_uuid id, void* output)
    -> Negotiation::Status;

static auto route(void*, perimortem_view_bytes) -> ttx_abstract {
  const ttx_abstract absent = Providers::None::get_abstract();
  return absent;
}

static auto visit(void*, ttx_graph_visitor) -> void {}

static auto reachable(void*) -> ttx_abstract_reachability {
  return TTX_ABSTRACT_REACHABLE;
}

static constexpr ttx_abstract_ops operations = {
  supports, bind, route, visit, reachable, Providers::Resolution::resolve};

static auto bind(void* context, perimortem_uuid id, void* output)
    -> Negotiation::Status {
  static constexpr ttx_alias_ops alias_operations = {
    get_referent, set_referent};

  const Identity::Uuid contract(id);
  if (contract == Abstract::contract_id) {
    *Data::cast<ttx_abstract>(output) = {context, &operations};
    return Negotiation::Status::Satisfied;
  }

  if (contract == Policies::Alias::contract_id) {
    *Data::cast<ttx_alias>(output) = {context, &alias_operations};
    return Negotiation::Status::Satisfied;
  }

  return Negotiation::Status::Rejected;
}

Providers::Alias::Alias() : referent(Providers::Unknown::get_abstract()) {}

Providers::Alias::Alias(Abstract initial) : Alias() {
  set_referent(initial);
}

auto Providers::Alias::set_referent(Abstract target) -> Negotiation::Status {
  return ::set_referent(&referent, target);
}

auto Providers::Alias::get_abstract() & -> Abstract {
  return Abstract(&referent, operations);
}
