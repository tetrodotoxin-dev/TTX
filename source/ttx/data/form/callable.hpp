// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/vector.hpp"

#include "ttx/data/form/schema.h"

namespace Ttx::Data::Form {

// Callable describes a function's calling convention, result and ordered
// arguments. An unset result denotes void. Argument entries can repeat a
// parameter type, while a variadic signature describes only its fixed prefix.
// The argument records and their referenced schemas remain available and
// unchanged through compilation. Copying Callable copies the C record and
// continues to borrow those arrays.
class Callable {
 public:
  enum class Convention : U32 {
    SystemVAMD64 = TTX_SCHEMA_SYSTEM_V_AMD64,
    SystemVAMD64Variadic = TTX_SCHEMA_SYSTEM_V_AMD64_VARIADIC,
    EmscriptenWasm32 = TTX_SCHEMA_EMSCRIPTEN_WASM32,
    EmscriptenWasm32Variadic = TTX_SCHEMA_EMSCRIPTEN_WASM32_VARIADIC,
    WindowsX64 = TTX_SCHEMA_WINDOWS_X64,
    WindowsX64Variadic = TTX_SCHEMA_WINDOWS_X64_VARIADIC,
    Native = TTX_SCHEMA_NATIVE_CONVENTION,
    NativeVariadic = TTX_SCHEMA_NATIVE_VARIADIC,
  };

  constexpr Callable(ttx_schema_callable value) : value(value) {}

  constexpr auto get_arguments() const
      -> Perimortem::Core::View::Vector<ttx_schema_argument> {
    if (!value.arguments || !value.count) {
      return {};
    }

    return {value.arguments[0], value.count};
  }

  constexpr auto get_result() const -> ttx_schema_reference {
    return value.result;
  }

  constexpr auto get_convention() const -> Convention {
    return Convention(value.convention);
  }

  constexpr auto get_abi() const -> const ttx_schema_callable& { return value; }

 private:
  ttx_schema_callable value;
};

}  // namespace Ttx::Data::Form
