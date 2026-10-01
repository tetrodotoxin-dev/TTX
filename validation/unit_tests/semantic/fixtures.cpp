// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#include "validation/unit_tests/semantic/fixtures.hpp"

#include "ttx/abi/receiver.hpp"
#include "ttx/data/protocol/consumer.hpp"
using namespace Validation::FlowTests;
Toolchain::Validation::Harness Validation::FlowTests::TtxFlow = {
  .name = "TTX::Flow",
};

auto Validation::FlowTests::Reader::query() -> Query {
  using namespace Ttx::Semantic::Negotiation;
  using namespace Ttx::Semantic::Transport;
  return Query(ttx_semantic_query(
      this,
      [](void* source, perimortem_uuid requested,
         ttx_storage result) -> ttx_binding_status {
        auto& reader = Ttx::Abi::Receiver::get<Reader>(source);
        const auto schema = +[](void* source) -> const Representation* {
          auto& reader = Ttx::Abi::Receiver::get<Reader>(source);
          ++reader.descriptions;
          return &reader.schema;
        };
        static const Ttx::Data::Protocol::Consumer::Operations direct =
            Ttx::Data::Protocol::Consumer::Operations(
                [](void* source) -> const Representation* {
                  auto& reader = Ttx::Abi::Receiver::get<Reader>(source);
                  ++reader.descriptions;
                  return reader.direct_schema ? reader.direct_schema
                                              : &reader.schema;
                });
        static const Ttx::Data::Protocol::Consumer::Operations operations =
            Ttx::Data::Protocol::Consumer::Operations(schema);
        const Perimortem::System::Uuid id(requested);
        if (id == Ttx::Semantic::Transport::Flow::direct.consumer) {
          ++reader.binds[0];
          if (!(reader.provides & PROVIDES_DIRECT)) {
            return static_cast<ttx_binding_status>(reader.decline);
          }
          return static_cast<ttx_binding_status>(
              Binding::provide<Ttx::Data::Protocol::Consumer>(
                  Ttx::Data::Protocol::Consumer::Api(source, &direct),
                  Ttx::Data::Form::Storage(result)));
        } else if (id == Ttx::Semantic::Transport::Flow::shared.consumer) {
          ++reader.binds[1];
          if (!(reader.provides & PROVIDES_SHARED)) {
            return static_cast<ttx_binding_status>(reader.decline);
          }
          return static_cast<ttx_binding_status>(
              Binding::provide<Ttx::Data::Protocol::Consumer>(
                  Ttx::Data::Protocol::Consumer::Api(source, &operations),
                  Ttx::Data::Form::Storage(result)));
        } else if (id == Ttx::Semantic::Transport::Flow::block.consumer) {
          ++reader.binds[2];
          if (!(reader.provides & PROVIDES_BLOCK)) {
            return static_cast<ttx_binding_status>(reader.decline);
          }
          return static_cast<ttx_binding_status>(
              Binding::provide<Ttx::Data::Protocol::Consumer>(
                  Ttx::Data::Protocol::Consumer::Api(source, &operations),
                  Ttx::Data::Form::Storage(result)));
        } else if (id == Ttx::Semantic::Transport::Flow::fragment.consumer) {
          ++reader.binds[3];
          if (!(reader.provides & PROVIDES_FRAGMENT)) {
            return static_cast<ttx_binding_status>(reader.decline);
          }
          return static_cast<ttx_binding_status>(
              Binding::provide<Ttx::Data::Protocol::Consumer>(
                  Ttx::Data::Protocol::Consumer::Api(source, &operations),
                  Ttx::Data::Form::Storage(result)));
        } else {
          return TTX_BINDING_UNKNOWN;
        }
      },
      [](void* source, perimortem_uuid requested) -> ttx_binding_status {
        auto& reader = Ttx::Abi::Receiver::get<Reader>(source);
        const Perimortem::System::Uuid id(requested);
        U8 protocol = 0;
        if (id == Ttx::Semantic::Transport::Flow::direct.consumer) {
          protocol = PROVIDES_DIRECT;
        } else if (id == Ttx::Semantic::Transport::Flow::shared.consumer) {
          protocol = PROVIDES_SHARED;
        } else if (id == Ttx::Semantic::Transport::Flow::block.consumer) {
          protocol = PROVIDES_BLOCK;
        } else if (id == Ttx::Semantic::Transport::Flow::fragment.consumer) {
          protocol = PROVIDES_FRAGMENT;
        } else {
          return TTX_BINDING_UNKNOWN;
        }

        return reader.provides & protocol
                   ? TTX_BINDING_SATISFIED
                   : static_cast<ttx_binding_status>(reader.decline);
      }));
}
