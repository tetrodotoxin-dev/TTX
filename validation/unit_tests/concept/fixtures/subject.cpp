// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/concept/fixtures/subject.hpp"

#include "toolchain/export.h"
#include "ttx/semantic/negotiation/library.h"

C_LINKAGE EXPORTED(TTX_TEST)
ttx_abstract abstract_subject() {
  static Validation::ConceptTests::Subject subject;
  return Ttx::Concept::Abstract::provide(subject).get_abi();
}

// This entry lends its provider only for the observation. The host decides
// whether to admit it, and the consumer can independently decline the result.
C_LINKAGE EXPORTED(TTX_TEST)
ttx_binding_status ttx_query(
    ttx_semantic_query host,
    ttx_query_receiver receive) {
  const perimortem_uuid admission =
      perimortem_uuid(0x5ef6275871544cf7ULL, 0xb3ee8d4b319b4b75ULL);
  const auto status = host.supports(host.source, admission);
  if (status != TTX_BINDING_SATISFIED) {
    return status;
  }
  Validation::ConceptTests::Subject subject;
  return receive.receive(
      receive.source, Ttx::Concept::Abstract::provide(subject).get_query());
}
