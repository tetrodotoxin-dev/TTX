// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/capabilities/import.hpp"

using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

C_LINKAGE const ttx_representation* ttx_import_representation() {
  return &Binding::representation<Capabilities::Import>();
}
