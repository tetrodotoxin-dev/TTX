// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/capabilities/create.hpp"

using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

C_LINKAGE const ttx_representation* ttx_create_representation() {
  return &Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
      Capabilities::Create::Api>::reference>::get_representation();
}
