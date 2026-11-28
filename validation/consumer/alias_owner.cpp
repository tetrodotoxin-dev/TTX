// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/policies/alias.hpp"
#include "ttx/graph/policies/none.h"
#include "ttx/graph/providers/alias.hpp"
#include "ttx/graph/providers/promise.hpp"
#include "validation/consumer/alias.h"

using namespace Ttx::Graph;
using namespace Ttx::Semantic;

template <typename Location>
concept CanBindAliasWithAnotherId = requires(Location location) {
  location.template bind<Policies::Alias>(Abstract::contract_id);
};

static_assert(!CanBindAliasWithAnotherId<Abstract>);
static_assert(!__is_convertible(Providers::Alias&&, Abstract));

auto consume_c_provider(
    ttx_abstract record,
    ttx_abstract initial,
    ttx_abstract replacement) -> S32 {
  const Abstract location = record;
  return location.bind<Policies::Alias>().visit(
      [&](Policies::Alias access) -> S32 {
        const Abstract captured = access.get_referent();
        if (captured != initial || access.get_abi().context == record.context) {
          return 11;
        }

        if (access.set_referent(replacement) !=
                Negotiation::Status::Satisfied ||
            access.set_referent(captured) != Negotiation::Status::Rejected) {
          return 12;
        }

        return location.bind<Policies::Alias>().visit(
            [&](Policies::Alias fresh) -> S32 {
              return fresh.get_referent() == replacement &&
                             access.get_referent() == fresh.get_referent()
                         ? 0
                         : 13;
            },

            []() -> S32 { return 14; });
      },

      []() -> S32 { return 10; });
}

S32 main() {
  Providers::Promise promise;
  const auto captured = promise.get_abstract();
  if (validate_promise(captured, 1) ||
      promise.fulfill(ttx_none()) != Negotiation::Status::Satisfied ||
      validate_promise(captured, 0)) {
    return 15;
  }

  Providers::Alias owner;
  return validate_alias(owner.get_abstract());
}
