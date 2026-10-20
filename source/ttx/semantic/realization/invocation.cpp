// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/realization/invocation.hpp"

using namespace Ttx::Data;

C_LINKAGE auto ttx_invocation_representation() -> const ttx_representation* {
  return &Form::Compiled<
              Form::Native<ttx_invocation>::reference>::get_representation()
              .get_abi();
}
