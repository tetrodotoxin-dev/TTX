// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/semantic/fixtures.hpp"

#include "ttx/data/protocol/consumer.hpp"

using namespace Validation::FlowTests;

Toolchain::Validation::Harness Validation::FlowTests::TtxFlow = {
  .name = "TTX::Flow",
};

auto Consumer::describe(void* context) -> const Representation* {
  auto& consumer = *static_cast<Consumer*>(context);
  ++consumer.descriptions;
  return &consumer.representation;
}

// A different Direct description lets the suite check that a representation
// mismatch leaves the other protocols available for negotiation.
auto Consumer::describe_direct(void* context) -> const Representation* {
  auto& consumer = *static_cast<Consumer*>(context);
  ++consumer.descriptions;
  return consumer.direct_representation ? consumer.direct_representation
                                        : &consumer.representation;
}

auto Consumer::bind(
    void* context,
    perimortem_uuid requested,
    ttx_storage result) -> ttx_binding_status {
  auto& consumer = *static_cast<Consumer*>(context);
  const Perimortem::System::Uuid id(requested);
  Count index;
  U8 protocol;
  if (id == Flow::direct.consumer) {
    index = 0;
    protocol = PROVIDES_DIRECT;
  } else if (id == Flow::shared.consumer) {
    index = 1;
    protocol = PROVIDES_SHARED;
  } else if (id == Flow::block.consumer) {
    index = 2;
    protocol = PROVIDES_BLOCK;
  } else if (id == Flow::fragment.consumer) {
    index = 3;
    protocol = PROVIDES_FRAGMENT;
  } else {
    return TTX_BINDING_UNKNOWN;
  }

  // Count refused attempts too so the tests can observe negotiation order.
  ++consumer.binds[index];
  if (!(consumer.provides & protocol)) {
    return static_cast<ttx_binding_status>(consumer.decline);
  }

  static const Ttx::Data::Protocol::Consumer::Operations direct = {
    describe_direct};
  static const Ttx::Data::Protocol::Consumer::Operations operations = {
    describe};
  return static_cast<ttx_binding_status>(
      Binding::provide<Ttx::Data::Protocol::Consumer>(
          {context, index == 0 ? &direct : &operations}, Storage(result)));
}

auto Consumer::supports(void* context, perimortem_uuid requested)
    -> ttx_binding_status {
  auto& consumer = *static_cast<Consumer*>(context);
  const Perimortem::System::Uuid id(requested);
  U8 protocol;
  if (id == Flow::direct.consumer) {
    protocol = PROVIDES_DIRECT;
  } else if (id == Flow::shared.consumer) {
    protocol = PROVIDES_SHARED;
  } else if (id == Flow::block.consumer) {
    protocol = PROVIDES_BLOCK;
  } else if (id == Flow::fragment.consumer) {
    protocol = PROVIDES_FRAGMENT;
  } else {
    return TTX_BINDING_UNKNOWN;
  }

  return consumer.provides & protocol
             ? TTX_BINDING_SATISFIED
             : static_cast<ttx_binding_status>(consumer.decline);
}

auto Consumer::query() -> Query {
  return Query({this, bind, supports});
}
