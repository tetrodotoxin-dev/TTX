// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"

namespace Ttx::Graph::Providers {

// Promise publishes a borrowed graph handle before its subject is available.
// The owner keeps the Promise at a stable address through every outstanding
// access and orders fulfillment against observations. Each selected subject,
// including another pending Promise, must remain accessible through its uses.
// A cycle in the chosen dependencies leaves the Promise pending. During
// foreign delegation a repeated outstanding operation on the same slot and
// argument receives the pending response. A delivered visitor callback can
// start a fresh observation through the same handle.
class Promise {
 public:
  Promise() = default;
  Promise(const Promise&) = delete;
  auto operator=(const Promise&) -> Promise& = delete;

  EXPORTED(TTX) auto get_abstract() & -> Abstract;

  // Fulfillment stores one complete subject. Repeating the same record is
  // harmless. An incomplete record or a different second commitment is
  // rejected. A known Unknown leaves the obligation pending.
  EXPORTED(TTX) auto fulfill(Abstract subject) -> Semantic::Negotiation::Status;

 private:
  ttx_abstract subject{};
};

}  // namespace Ttx::Graph::Providers
