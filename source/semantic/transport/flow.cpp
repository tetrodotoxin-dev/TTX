// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#include "ttx/semantic/transport/flow.hpp"

#include "ttx/data/protocol/consumer.hpp"
#include "ttx/data/protocol/direct/provider.hpp"
#include "ttx/data/protocol/shared/lifetime.hpp"
#include "ttx/data/protocol/shared/provider.hpp"

using namespace Ttx;
using namespace Ttx::Semantic::Transport;
using namespace Ttx::Semantic::Negotiation;
using Ttx::Data::Form::Representation;
using Ttx::Data::Form::Storage;

static auto consumer_supports(const void*, perimortem_uuid contract)
    -> ttx_binding_status {
  const Perimortem::System::Uuid id(contract);
  return id == Flow::direct.consumer || id == Flow::shared.consumer ||
                 id == Flow::block.consumer || id == Flow::fragment.consumer
             ? TTX_BINDING_SATISFIED
             : TTX_BINDING_UNSUPPORTED;
}

auto ttx_flow_consumer(const ttx_representation* required)
    -> ttx_semantic_query {
  // One representation requirement serves every accepted transport. The UUID
  // selects permission to use that transport, so sharing the API does not make
  // another consumer policy accept protocols it has not advertised.
  static const Data::Protocol::Consumer::Operations operations = {
    [](const void* source) -> const ttx_representation* {
      return static_cast<const ttx_representation*>(source);
    },
  };
  return {
    required,
    [](const void* source, perimortem_uuid contract,
       ttx_storage requested) -> ttx_binding_status {
      const auto status = consumer_supports(source, contract);
      if (status != TTX_BINDING_SATISFIED) {
        return status;
      }
      return static_cast<ttx_binding_status>(
          Binding::provide<Data::Protocol::Consumer>(
              Data::Protocol::Consumer::Api(source, &operations),
              Storage(requested)));
    },
    consumer_supports,
  };
}

static auto failure(Ttx::Semantic::Negotiation::Binding::Failure status)
    -> Flow::Status {
  switch (status) {
  case Ttx::Semantic::Negotiation::Binding::Failure::Unsupported:
    return Flow::Status::Unsupported;
  case Ttx::Semantic::Negotiation::Binding::Failure::Pending:
    return Flow::Status::BindingPending;
  default:
    return Flow::Status::Rejected;
  }
}

// A candidate needs both bindings and an agreed payload ABI. Unsupported or
// incompatible candidates let us try another independent protocol, while a
// pending or rejected binding preserves the owner's decision. Comparing the
// ABI here keeps data access and lifetime acquisition after that agreement.
template <typename Provider, typename Install>
static auto cooperate(
    Ttx::Semantic::Negotiation::Query consumer,
    Ttx::Semantic::Negotiation::Query provider,
    Flow::Contracts contracts,
    Install install) -> Flow::Status {
  return consumer.bind<Data::Protocol::Consumer>(contracts.consumer)
      .visit(
          [&](auto target) {
            return provider.bind<Provider>(contracts.provider)
                .visit(
                    [&](auto source) {
                      const auto& representation = target.get_representation();
                      if (!representation.compatible(
                              source.get_representation())) {
                        return Flow::Status::Incompatible;
                      }

                      install(representation, source);
                      return Flow::Status::Success;
                    },
                    failure);
          },
          failure);
}

auto Flow::connect(
    Ttx::Semantic::Negotiation::Query consumer,
    Ttx::Semantic::Negotiation::Query provider) -> Status {
  Status unavailable = Status::Unsupported;
  auto next = [&](Status result) {
    if (result == Status::Incompatible) {
      unavailable = result;
    }

    return result == Status::Unsupported || result == Status::Incompatible;
  };

  auto result = cooperate<Data::Protocol::Direct::Provider>(
      consumer, provider, direct,
      [&](const Representation& agreed, auto source) {
        representation = &agreed;
        protocol = Protocol::Direct;
        state.direct = source.read_ptr();
      });
  if (!next(result)) {
    return result;
  }

  // Select Shared before acquiring it. A provider failure belongs to that
  // agreement and cannot silently choose a different transport afterward.
  Status acquired = Status::Rejected;
  result = cooperate<Data::Protocol::Shared::Provider>(
      consumer, provider, shared,
      [&](const Representation& agreed, auto source) {
        acquired = source.acquire().visit(
            [&](Data::Protocol::Shared::Lifetime& lifetime) {
              representation = &agreed;
              protocol = Protocol::Shared;
              state.shared = lifetime.take_abi();
              return Status::Success;
            },
            [](Ttx::Data::Status status) {
              return static_cast<Status>(status);
            });
      });
  if (result == Status::Success) {
    return acquired;
  }

  if (!next(result)) {
    return result;
  }

  result = cooperate<Data::Protocol::Block::Provider>(
      consumer, provider, block,
      [&](const Representation& agreed, auto source) {
        representation = &agreed;
        protocol = Protocol::Block;
        state.block = source.get_abi();
      });
  if (!next(result)) {
    return result;
  }

  result = cooperate<Data::Protocol::Fragment::Provider>(
      consumer, provider, fragment,
      [&](const Representation& agreed, auto source) {
        representation = &agreed;
        protocol = Protocol::Fragment;
        state.fragment = source.get_abi();
      });
  return result == Status::Unsupported ? unavailable : result;
}

auto Flow::close() -> void {
  ttx_shared_lifetime lifetime = {};
  if (protocol == Protocol::Shared) {
    lifetime = state.shared;
  }

  protocol = Protocol::None;
  representation = nullptr;
  // Unpublish before entering the owner's release hook. That hook can then
  // establish another agreement without this close overwriting it on return.
  ttx_shared_release(&lifetime);
}

auto ttx_flow_connect(
    ttx_flow* flow,
    ttx_semantic_query consumer,
    ttx_semantic_query provider) -> ttx_flow_status {
  return static_cast<ttx_flow_status>(reinterpret_cast<Flow*>(flow)->connect(
      Ttx::Semantic::Negotiation::Query(consumer),
      Ttx::Semantic::Negotiation::Query(provider)));
}
