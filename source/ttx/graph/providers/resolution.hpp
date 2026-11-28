// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#define TOOLCHAIN_PRIVATE_HEADER 1

#include "perimortem/core/data.hpp"

#include "ttx/graph/abstract.h"

namespace Ttx::Graph::Providers {

// Alias and Promise store one borrowed Abstract at the callback context used
// by Resolution::resolve. The shared callback identifies records whose links
// the resolver can read directly. For other Promise operations the walk
// stops at an Alias so that its local bindings remain available.
class Resolution {
 public:
  static auto complete(ttx_abstract value) -> Bool {
    const auto* ops = value.operations;
    return value.context && ops && ops->supports && ops->bind &&
           ops->resolve_route && ops->visit_routes && ops->get_reachability &&
           ops->resolve;
  }

  // Brent's cycle detector retains one earlier handle and doubles the
  // distance between checkpoints. Revisiting that handle returns an empty
  // internal record. A finite chain reads each link once, including the
  // last link before an independently supplied operation table.
  template <typename Follows>
  static auto follow(ttx_abstract current, Follows follows) -> ttx_abstract {
    auto checkpoint = current;
    Count interval = 1;
    Count distance = 0;
    while (current.operations && follows(current)) {
      current = *Perimortem::Core::Data::cast<ttx_abstract>(current.context);
      if (current.context == checkpoint.context &&
          current.operations == checkpoint.operations) {
        return {};
      }

      ++distance;
      if (distance == interval) {
        checkpoint = current;
        interval *= 2;
        distance = 0;
      }
    }

    return current;
  }

  static auto resolve(ttx_abstract self) -> ttx_abstract;
};

}  // namespace Ttx::Graph::Providers
