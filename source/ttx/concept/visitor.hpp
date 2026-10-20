// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/bytes.hpp"

namespace Ttx::Concept {

// Visitor passes a route and a value to the consumer's callback. The provider
// chooses which values to visit and can read them directly from its own data.
// Value can be a native reference or a negotiated API record.
//
// Enumeration is synchronous. The caller keeps the callback alive until
// enumeration returns, and the provider completes every callback in that time.
// The enumeration contract specifies how long each route and value can be used.
template <typename Value>
class Visitor {
 public:
  constexpr Visitor(
      void* context,
      void (*callback)(void*, Perimortem::Core::View::Bytes, Value))
      : context(context), callback(callback) {}

  template <typename Function>
    requires(!__is_same(__remove_cvref(Function), Visitor))
  // Stores a reference to callback, which the caller keeps alive until
  // visitation returns. Explicit construction makes that borrowed reference
  // visible at the call site.
  constexpr explicit Visitor(Function& callback)
      : Visitor(&callback, invoke<Function>) {}

  auto operator()(Perimortem::Core::View::Bytes route, Value value) const
      -> void {
    callback(context, route, value);
  }

 private:
  // The constructor stores &callback as context. The caller keeps that
  // Function alive until all calls through this Visitor have finished.
  template <typename Function>
  static auto
      invoke(void* context, Perimortem::Core::View::Bytes route, Value value)
          -> void {
    (*Perimortem::Core::Data::cast<Function>(context))(route, value);
  }

  void* context;
  void (*callback)(void*, Perimortem::Core::View::Bytes, Value);
};

}  // namespace Ttx::Concept
