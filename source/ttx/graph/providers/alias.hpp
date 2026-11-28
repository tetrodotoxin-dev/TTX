// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"

namespace Ttx::Graph::Providers {

// Alias owns one redirectable graph reference. Publishing it as an Abstract
// borrows its slot. Copies of that Abstract follow the same slot, so the
// owner must outlive every such handle and every binding that uses the slot.
// A captured referent keeps its own provider and access duration.
// The owner synchronizes reads with writes to the slot. During a resolve
// call, owners keep all traversed links stable until delegation returns.
class Alias {
 public:
  EXPORTED(TTX) Alias();

  // An implicit conversion from Abstract would give the caller a new graph
  // location each time it passed the same target. Explicit construction gives
  // that reference an owner. An incomplete initial target
  // leaves the new reference at Unknown.
  explicit EXPORTED(TTX) Alias(Abstract initial);
  Alias(const Alias&) = delete;
  auto operator=(const Alias&) -> Alias& = delete;

  EXPORTED(TTX) auto get_abstract() & -> Abstract;
  operator Abstract() & { return get_abstract(); }
  auto get_referent() const -> Abstract { return referent; }
  EXPORTED(TTX) auto set_referent(Abstract target)
      -> Semantic::Negotiation::Status;

 private:
  ttx_abstract referent;
};

}  // namespace Ttx::Graph::Providers
