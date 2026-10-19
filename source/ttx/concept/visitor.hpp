// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/bytes.hpp"

namespace Ttx::Concept {

// Discovery lends the caller's callback to the provider for one enumeration.
// The provider chooses which named values to expose and can produce each one
// directly from its own storage. Native references and bound views use the
// same callback shape without forcing either representation onto the other.
//
// The callback is borrowed, so the provider must finish calling it before
// enumeration returns. It cannot retain the Visitor for deferred work. Each
// discovery contract supplies the lifetime of the names and values it emits.
template <typename Value>
class Visitor {
 public:
  constexpr Visitor(
      void* context,
      void (*callback)(void*, Perimortem::Core::View::Bytes, Value))
      : context(context), callback(callback) {}

  template <typename Function>
    requires(!__is_same(__remove_cvref(Function), Visitor))
  // The callback stays alive through synchronous visitation. Explicit
  // construction exposes this borrowed reference at the call site.
  constexpr explicit Visitor(Function& callback)
      : Visitor(&callback, invoke<Function>) {}

  auto operator()(Perimortem::Core::View::Bytes route, Value value) const
      -> void {
    callback(context, route, value);
  }

 private:
  // Construction supplies the address of the caller's Function. Its lifetime
  // covers every invocation through this Visitor.
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
