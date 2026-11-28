// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/identity/uuid.hpp"

#include "ttx/graph/policies/promise.h"

namespace Ttx::Graph::Policies {

// Promise identifies a pending obligation through `supports<Promise>()`.
// While the owner is still choosing a subject, the consumer can keep the
// supplied Abstract and retry its original operation under the same access
// grant. After fulfillment that operation runs with the subject's context.
class Promise {
 public:
  static constexpr auto contract_id =
      Perimortem::Core::Identity::Uuid(TTX_PROMISE_ID_HIGH, TTX_PROMISE_ID_LOW);
};

}  // namespace Ttx::Graph::Policies
