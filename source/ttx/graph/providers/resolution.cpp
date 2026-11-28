// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/providers/resolution.hpp"

#include "ttx/graph/policies/unknown.h"

using namespace Ttx::Graph;

// A foreign resolver can call back through the Alias or Promise that asked
// for its subject. ResolutionCall retains that origin on this thread until
// the foreign operation returns. Revisiting it yields Unknown for the cycle.
struct ResolutionCall {
  ttx_abstract origin;
  ResolutionCall* previous;
  inline static thread_local ResolutionCall* active = nullptr;

  // Explicit construction registers this origin for the lifetime of the
  // foreign call. An implicit conversion would install a temporary entry.
  explicit ResolutionCall(ttx_abstract self) : origin(self), previous(active) {
    active = this;
  }

  ~ResolutionCall() { active = previous; }
};

auto Providers::Resolution::resolve(ttx_abstract self) -> ttx_abstract {
  for (auto* call = ResolutionCall::active; call; call = call->previous) {
    if (call->origin.context == self.context &&
        call->origin.operations == self.operations) {
      return ttx_unknown();
    }
  }

  const auto subject = follow(self, [](ttx_abstract link) {
    return link.operations->resolve == resolve;
  });

  if (!subject.operations) {
    return ttx_unknown();
  }

  ResolutionCall call(self);
  return subject.operations->resolve(subject);
}
