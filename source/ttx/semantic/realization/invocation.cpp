// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/realization/invocation.hpp"

using namespace Ttx::Data::Form;

C_LINKAGE const ttx_representation* ttx_invocation_representation() {
  return &Compiled<Native<ttx_invocation>::reference>::get_representation();
}
