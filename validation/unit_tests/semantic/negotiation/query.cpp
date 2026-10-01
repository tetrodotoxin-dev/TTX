// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/static/vector.hpp"

#include "validation/unit_tests/semantic/fixtures.hpp"
#include "ttx/data/protocol/direct/provider.hpp"

using namespace Validation::FlowTests;

// Two semantic promises can share one Data API. Supplying the requested UUID
// explicitly must preserve the provider's policy instead of deriving identity
// from the C++ facade or accepting another promise with the same byte shape.
VALIDATION_TEST(TtxFlow, explicit_contract) {
  using Ttx::Data::Protocol::Direct::Provider;
  static constexpr Perimortem::System::Uuid selected =
      Perimortem::System::Uuid(41, 73);
  static const auto& form = Ttx::Data::Form::Compiled<
      Ttx::Data::Form::Native<U32>::reference>::get_representation();
  U32 payload = 42;
  const Query query(ttx_semantic_query(
      &payload,
      [](void* source, perimortem_uuid contract,
         ttx_storage requested) -> ttx_binding_status {
        if (Perimortem::System::Uuid(contract) != selected) {
          return TTX_BINDING_REJECTED;
        }

        static const Provider::Operations operations = Provider::Operations(
            [](void*) -> const Representation* { return &form; },
            [](void* source) -> const void* { return source; });
        return static_cast<ttx_binding_status>(Binding::provide<Provider>(
            Provider::Api(source, &operations), Storage(requested)));
      },
      [](void*, perimortem_uuid contract) -> ttx_binding_status {
        return Perimortem::System::Uuid(contract) == selected
                   ? TTX_BINDING_SATISFIED
                   : TTX_BINDING_REJECTED;
      }));

  query.bind<Provider>(selected).visit(
      [&](Provider provider) {
        EXPECT_EQ(*static_cast<const U32*>(provider.read_ptr()), U32(42));
      },
      [&](Binding::Failure) { EXPECT(false); });
  query.bind<Provider>(Ttx::Semantic::Transport::Flow::direct.provider)
      .visit(
          [&](Provider) { EXPECT(false); },
          [&](Binding::Failure failure) {
            EXPECT(failure == Binding::Failure::Rejected);
          });
}

// The module entry is the prearranged C bootstrap. It supplies a writer for
// the Query record, while the host's reader permits only Direct or Shared.
// The agreed C representation grants the cast that imports its bind thunk.
// That imported Query then participates in ordinary protocol negotiation.
VALIDATION_TEST(TtxFlow, bootstrap_bind) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  // Derive the expected callable form from the actual C declaration. The C
  // provider authors the same form independently, including bind's signature.
  const auto& query_schema = Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
      ttx_semantic_query>::reference>::get_representation();
  Validation::FlowTests::Reader receiver = Validation::FlowTests::Reader(
      query_schema, PROVIDES_DIRECT | PROVIDES_SHARED);

  Flow bootstrap;
  ASSERT(
      bootstrap.connect(receiver.query(), module.bootstrap_writer()) ==
      Flow::Status::Success);
  EXPECT(bootstrap.get_protocol() == Protocol::Direct);

  const auto imported = module.import_query(bootstrap);
  EXPECT(imported.is_set());
  EXPECT(
      imported.supports(Ttx::Semantic::Transport::Flow::direct.provider) ==
      Ttx::Semantic::Negotiation::Binding::Status::Satisfied);
  EXPECT(
      imported.supports(Ttx::Semantic::Transport::Flow::block.provider) ==
      Ttx::Semantic::Negotiation::Binding::Status::Unknown);
  Validation::FlowTests::Reader data = Validation::FlowTests::Reader(four);
  Flow flow;
  ASSERT(flow.connect(data.query(), imported) == Flow::Status::Success);

  Static::Vector<U32, 4> target;
  ASSERT(Copy::flow(flow, storage(four, target)) == Status::Success);
  EXPECT_EQ(target[3], U32(4));
}
