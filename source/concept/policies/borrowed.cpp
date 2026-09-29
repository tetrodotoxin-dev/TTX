// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/borrowed.hpp"

PERIMORTEM_C const ttx_representation* ttx_borrowed_representation() {
  return &Ttx::Semantic::Negotiation::Binding::representation<
      Ttx::Concept::Policies::Borrowed>();
}
