// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/negotiation/callback.h"
#include "ttx/semantic/negotiation/query.hpp"

namespace Ttx::Semantic::Negotiation {

// Adapts a C++ function to ttx_query_callback. The callback uses the supplied
// function by reference, so the function stays alive through the provider call.
class Callback {
 public:
  explicit constexpr Callback(ttx_query_callback api) : api(api) {}

  template <typename Function>
    requires(!__is_same(__remove_cvref(Function), Callback))
  explicit constexpr Callback(Function& function)
      : api(&function, invoke<Function>) {}

  constexpr auto get_abi() const -> ttx_query_callback { return api; }

  auto operator()(Query query) const -> Binding::Status {
    return static_cast<Binding::Status>(api.callback(api.context, query));
  }

 private:
  template <typename Function>
  static auto invoke(void* context, ttx_semantic_query query)
      -> ttx_binding_status {
    return static_cast<ttx_binding_status>(
        (*static_cast<Function*>(context))(Query(query)));
  }

  ttx_query_callback api;
};

}  // namespace Ttx::Semantic::Negotiation

TTX_DATA_RECORD(
    ttx_query_callback,
    TTX_DATA_MEMBER(ttx_query_callback, context),
    TTX_DATA_MEMBER(ttx_query_callback, callback));
