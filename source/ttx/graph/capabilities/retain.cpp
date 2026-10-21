// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/capabilities/retain.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;

auto Capabilities::Retain::retain() const
    -> Storage::Result<Policies::Retained, Negotiation::Status> {
  const auto api = get_abi();
  ttx_retained output = ttx_retained();
  const auto status = api.operations->retain(api.context, &output);
  if (status == TTX_NEGOTIATION_SATISFIED) {
    return Policies::Retained(output);
  }

  return status == TTX_NEGOTIATION_UNKNOWN ? Negotiation::Status::Unknown
                                           : Negotiation::Status::Rejected;
}
