// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/providers/none.hpp"

#include "ttx/graph/policies/none.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;

static auto supports(void*, perimortem_uuid id) -> Negotiation::Status {
  const Identity::Uuid contract(id);
  return contract == Abstract::contract_id ||
                 contract == Policies::None::contract_id
             ? Negotiation::Status::Satisfied
             : Negotiation::Status::Rejected;
}

static auto bind(void*, perimortem_uuid id, void* output)
    -> Negotiation::Status {
  if (Identity::Uuid(id) != Abstract::contract_id) {
    return Negotiation::Status::Rejected;
  }

  *Data::cast<ttx_abstract>(output) = Providers::None::get_abstract();
  return Negotiation::Status::Satisfied;
}

static auto route(void*, perimortem_view_bytes) -> ttx_abstract {
  const ttx_abstract location = Providers::None::get_abstract();
  return location;
}

static auto visit(void*, ttx_graph_visitor) -> void {}

static auto reachability(void*) -> ttx_abstract_reachability {
  return TTX_ABSTRACT_UNREACHABLE;
}

auto Providers::None::get_abstract() -> Abstract {
  static None provider;
  static constexpr ttx_abstract_ops operations = {
    supports, bind, route, visit, reachability};

  return Abstract(&provider, operations);
}
