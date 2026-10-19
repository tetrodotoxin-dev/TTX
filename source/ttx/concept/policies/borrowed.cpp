// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/borrowed.hpp"

C_LINKAGE const ttx_representation* ttx_borrowed_representation() {
  return &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
      Ttx::Concept::Policies::Borrowed::Api>::reference>::get_representation();
}
