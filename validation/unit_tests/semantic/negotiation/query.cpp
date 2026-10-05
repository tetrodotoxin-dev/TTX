// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/semantic/fixtures.hpp"

#include "perimortem/core/static/vector.hpp"

#include "ttx/data/protocol/direct/provider.hpp"

using namespace Validation::FlowTests;

// Two semantic promises can share one Data API. Supplying the requested UUID
// explicitly must preserve the provider's policy instead of deriving identity
// from the C++ interface or accepting another promise with the same byte shape.
VALIDATION_TEST(TtxFlow, explicit_contract) {
  using Interface = Ttx::Data::Protocol::Direct::Provider;
  struct Provider {
    static auto selected() -> Perimortem::System::Uuid {
      return Perimortem::System::Uuid(41, 73);
    }

    static auto representation(void*) -> const Representation* {
      return &Ttx::Data::Form::Compiled<
          Ttx::Data::Form::Native<U32>::reference>::get_representation();
    }

    static auto read(void* context) -> const void* { return context; }

    static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
      return Perimortem::System::Uuid(id) == selected() ? TTX_BINDING_SATISFIED
                                                        : TTX_BINDING_REJECTED;
    }

    static auto bind(void* context, perimortem_uuid id, ttx_storage output)
        -> ttx_binding_status {
      const auto status = supports(context, id);
      if (status != TTX_BINDING_SATISFIED) {
        return status;
      }

      static const Interface::Operations operations = {representation, read};
      return static_cast<ttx_binding_status>(
          Binding::provide<Interface>({context, &operations}, Storage(output)));
    }
  };
  const auto selected = Provider::selected();
  U32 payload = 42;
  const Query query({&payload, Provider::bind, Provider::supports});

  query.bind<Interface>(selected).visit(
      [&](Interface provider) {
        EXPECT_EQ(*static_cast<const U32*>(provider.read_ptr()), U32(42));
      },
      [&](Binding::Failure) { EXPECT(false); });
  query.bind<Interface>(Ttx::Semantic::Transport::Flow::direct.provider)
      .visit(
          [&](Interface) { EXPECT(false); },
          [&](Binding::Failure failure) {
            EXPECT(failure == Binding::Failure::Rejected);
          });
}

// The module entry is the prearranged C bootstrap. It supplies a provider for
// the Query record, while the host's consumer permits only Direct or Shared.
// The agreed C representation grants the cast that imports its bind function.
// That imported Query then participates in ordinary protocol negotiation.
VALIDATION_TEST(TtxFlow, bootstrap_bind) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  // Derive the expected callable form from the actual C declaration. The C
  // provider authors the same form independently, including bind's signature.
  const auto& query_representation =
      Ttx::Data::Form::Compiled<Ttx::Data::Form::Native<
          ttx_semantic_query>::reference>::get_representation();
  Validation::FlowTests::Consumer consumer = Validation::FlowTests::Consumer(
      query_representation, PROVIDES_DIRECT | PROVIDES_SHARED);

  Flow bootstrap;
  ASSERT(
      bootstrap.connect(consumer.query(), module.bootstrap_provider()) ==
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
  Validation::FlowTests::Consumer data = Validation::FlowTests::Consumer(four);
  Flow flow;
  ASSERT(flow.connect(data.query(), imported) == Flow::Status::Success);

  Static::Vector<U32, 4> target;
  ASSERT(Copy::flow(flow, storage(four, target)) == Status::Success);
  EXPECT_EQ(target[3], U32(4));
}
