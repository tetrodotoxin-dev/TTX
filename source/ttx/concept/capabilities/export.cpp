// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/capabilities/export.hpp"

using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

PERIMORTEM_C const ttx_representation* ttx_export_representation() {
  return &Binding::representation<Capabilities::Export>();
}
