// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tests/concept/fixtures/subject.hpp"

PERIMORTEM_C ttx_abstract abstract_subject() {
  static const Validation::ConceptTests::Subject subject;
  return Ttx::Concept::Abstract::provide(subject).get_abi();
}
