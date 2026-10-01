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

static auto failure(Ttx::Semantic::Negotiation::Binding::Failure status)
    -> Flow::Status {
  switch (status) {
  case Ttx::Semantic::Negotiation::Binding::Failure::Unknown:
    return Flow::Status::Unknown;
  default:
    return Flow::Status::Rejected;
  }
}

// A custom consumer and provider each bind their named API before payload
// agreement. Unknown leaves room for another protocol, as does a payload
// mismatch. Rejected stops the search when either participant refuses.
// Comparing the representations here keeps data access and lifetime acquisition
// after agreement.
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

template <typename Provider, typename Install>
static auto cooperate(
    Flow::Requirement consumer,
    Query provider,
    Flow::Contracts contracts,
    Install install) -> Flow::Status {
  // The built in requirement already accepts every consumer protocol, so only
  // the provider must bind before their representations can be compared.
  return provider.bind<Provider>(contracts.provider)
      .visit(
          [&](auto source) {
            const auto& representation = *consumer.representation;
            if (!representation.compatible(source.get_representation())) {
              return Flow::Status::Incompatible;
            }

            install(representation, source);
            return Flow::Status::Success;
          },
          failure);
}

template <typename Consumer>
auto Flow::connect_with(Consumer consumer, Query provider) -> Status {
  Status unavailable = Status::Unknown;
  auto next = [&](Status result) {
    if (result == Status::Incompatible) {
      unavailable = result;
    }

    return result == Status::Unknown || result == Status::Incompatible;
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
  return result == Status::Unknown ? unavailable : result;
}

auto Flow::connect(Query consumer, Query provider) -> Status {
  return connect_with(consumer, provider);
}

auto Flow::connect(Requirement consumer, Query provider) -> Status {
  if (!consumer.representation) {
    return Status::Invalid;
  }

  return connect_with(consumer, provider);
}

auto Flow::close() -> void {
  ttx_shared_lifetime lifetime = ttx_shared_lifetime();
  if (protocol == Protocol::Shared) {
    lifetime = state.shared;
  }

  protocol = Protocol::None;
  representation = nullptr;
  // Clear the Flow before entering the provider's release hook. That hook can
  // then establish another agreement without this close overwriting it on
  // return.
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

auto ttx_flow_connect_requirement(
    ttx_flow* flow,
    ttx_flow_requirement consumer,
    ttx_semantic_query provider) -> ttx_flow_status {
  return static_cast<ttx_flow_status>(reinterpret_cast<Flow*>(flow)->connect(
      Flow::Requirement{consumer.representation}, Query(provider)));
}
