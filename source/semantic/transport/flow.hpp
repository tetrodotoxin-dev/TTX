// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/system/uuid.hpp"

#include "ttx/data/protocol/block/provider.hpp"
#include "ttx/data/protocol/fragment/provider.hpp"
#include "ttx/data/protocol/shared/lifetime.hpp"
#include "ttx/semantic/negotiation/query.hpp"
#include "ttx/semantic/transport/flow.h"

namespace Ttx::Semantic::Transport {

// Flow selects one synchronous access contract and retains its bound state.
// The consumer describes a representation, while the provider decides which of
// Direct, Shared, Block and Fragment it can provide. Operations use the first
// compatible pair without repeating negotiation or discovering other roles.
//
// Establishment finishes before returning. Shared keeps its acquired lifetime
// until close, allowing several operations to use that same representation.
// The endpoints, representations and their code remain alive through Flow's
// uses. Execution policies such as deferred work wrap these calls outside Flow.
class Flow {
 public:
  // A pair identifies the two sides of one agreement. Their shared spelling
  // does not imply that either endpoint supplies the other's capabilities.
  struct Contracts {
    Perimortem::System::Uuid consumer;
    Perimortem::System::Uuid provider;
  };

  static constexpr Contracts direct = {
    Perimortem::System::Uuid{
      TTX_DIRECT_CONSUMER_ID_HIGH,
      TTX_DIRECT_CONSUMER_ID_LOW,
    },
    Perimortem::System::Uuid{
      TTX_DIRECT_PROVIDER_ID_HIGH,
      TTX_DIRECT_PROVIDER_ID_LOW,
    },
  };

  static constexpr Contracts shared = {
    Perimortem::System::Uuid{
      TTX_SHARED_CONSUMER_ID_HIGH,
      TTX_SHARED_CONSUMER_ID_LOW,
    },
    Perimortem::System::Uuid{
      TTX_SHARED_PROVIDER_ID_HIGH,
      TTX_SHARED_PROVIDER_ID_LOW,
    },
  };

  static constexpr Contracts block = {
    Perimortem::System::Uuid{
      TTX_BLOCK_CONSUMER_ID_HIGH,
      TTX_BLOCK_CONSUMER_ID_LOW,
    },
    Perimortem::System::Uuid{
      TTX_BLOCK_PROVIDER_ID_HIGH,
      TTX_BLOCK_PROVIDER_ID_LOW,
    },
  };

  static constexpr Contracts fragment = {
    Perimortem::System::Uuid{
      TTX_FRAGMENT_CONSUMER_ID_HIGH,
      TTX_FRAGMENT_CONSUMER_ID_LOW,
    },
    Perimortem::System::Uuid{
      TTX_FRAGMENT_PROVIDER_ID_HIGH,
      TTX_FRAGMENT_PROVIDER_ID_LOW,
    },
  };

  enum class Protocol : U8 { None, Direct, Shared, Block, Fragment };

  enum class Status : U8 {
    Success = TTX_DATA_SUCCESS,
    Invalid = TTX_DATA_INVALID,
    Bounds = TTX_DATA_BOUNDS,
    Overflow = TTX_DATA_OVERFLOW,
    Unsupported = TTX_DATA_UNSUPPORTED,
    Incompatible = TTX_DATA_INCOMPATIBLE,
    Denied = TTX_DATA_DENIED,
    Busy = TTX_DATA_BUSY,
    IoError = TTX_DATA_IO_ERROR,
    Rejected = TTX_FLOW_REJECTED,
    BindingPending = TTX_FLOW_BINDING_PENDING,
  };

  Flow() = default;
  Flow(const Flow&) = delete;

  auto operator=(const Flow&) -> Flow& = delete;
  ~Flow() { close(); }

  // Negotiation can precede destination allocation because the consumer only
  // needs to state which representation it accepts. Both overloads borrow
  // that representation. Later operations choose their own destination storage.
  static auto consumer(const Data::Form::Representation& representation)
      -> Negotiation::Query {
    return Negotiation::Query(ttx_flow_consumer(&representation));
  }

  static auto consumer(Data::Form::Storage storage) -> Negotiation::Query {
    return consumer(storage.get_representation());
  }

  // A returned success establishes a usable Flow. Another connection requires
  // closing that agreement first so a retained Shared lifetime is not replaced.
  auto connect(Negotiation::Query consumer, Negotiation::Query provider)
      -> Status;

  // Close ends the Shared lifetime after all operations using it have returned.
  auto close() -> void;

  auto get_protocol() const -> Protocol { return protocol; }

  auto get_representation() const -> const Data::Form::Representation& {
    return *representation;
  }

  auto get_abi() -> ttx_flow* { return reinterpret_cast<ttx_flow*>(this); }

  auto get_abi() const -> const ttx_flow* {
    return reinterpret_cast<const ttx_flow*>(this);
  }

  // A ready Flow already settled which protocol supplies the data. Visiting
  // lends that protocol's usable pointer or operations directly, so consumers
  // can perform their work without another bind or an assumption about the
  // provider's other capabilities. Only a successfully established Flow is
  // visitable.
  template <typename D, typename S, typename B, typename F>
  auto visit(D&& direct, S&& shared, B&& block, F&& fragment) const {
    switch (protocol) {
    case Protocol::Direct:
      return direct(state.direct);
    case Protocol::Shared:
      return shared(state.shared.data);
    case Protocol::Block:
      return block(Data::Protocol::Block::Provider(state.block));
    case Protocol::Fragment:
      return fragment(Data::Protocol::Fragment::Provider(state.fragment));
    default:
      // TODO: Decide whether to call an out of line fatal diagnostic here.
      // Log stays out of headers to avoid importing its source machinery. A
      // diagnostic member would preserve that boundary, but its effect on
      // valid visitation needs measuring before choosing that exception.
      __builtin_unreachable();
    }
  }

 private:
  union State {
    const void* direct;
    ttx_shared_lifetime shared;
    ttx_block_provider block;
    ttx_fragment_provider fragment;

    constexpr State() : direct(nullptr) {}
  } state;
  const Data::Form::Representation* representation = nullptr;
  Protocol protocol = Protocol::None;
};
}  // namespace Ttx::Semantic::Transport
