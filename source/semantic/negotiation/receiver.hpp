// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/negotiation/query.hpp"
#include "ttx/semantic/negotiation/receiver.h"

namespace Ttx::Semantic::Negotiation {

// Adapts a C++ function to ttx_query_receiver. The callback uses the supplied
// function by reference, so the function stays alive through the provider call.
class Receiver {
 public:
  explicit constexpr Receiver(ttx_query_receiver value) : value(value) {}
  template <typename Function>
    requires(!__is_same(__remove_cvref(Function), Receiver))
  explicit constexpr Receiver(Function& function)
      : value(&function, [](void* source, ttx_semantic_query query) {
          return static_cast<ttx_binding_status>(
              (*static_cast<Function*>(source))(Query(query)));
        }) {}
  constexpr auto get_abi() const -> ttx_query_receiver { return value; }
  auto operator()(Query subject) const -> Binding::Status {
    return static_cast<Binding::Status>(value.receive(value.source, subject));
  }

 private:
  ttx_query_receiver value;
};

}  // namespace Ttx::Semantic::Negotiation

TTX_DATA_RECORD(
    ttx_query_receiver,
    TTX_DATA_MEMBER(ttx_query_receiver, source),
    TTX_DATA_MEMBER(ttx_query_receiver, receive));
