// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

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
  constexpr explicit Visitor(Function& callback)
      : Visitor(&callback, invoke<Function>) {}

  auto operator()(Perimortem::Core::View::Bytes name, Value value) const
      -> void {
    callback(context, name, value);
  }

 private:
  template <typename Function>
  static void
      invoke(void* context, Perimortem::Core::View::Bytes name, Value value) {
    (*static_cast<Function*>(context))(name, value);
  }

  void* context;
  void (*callback)(void*, Perimortem::Core::View::Bytes, Value);
};

}  // namespace Ttx::Concept
