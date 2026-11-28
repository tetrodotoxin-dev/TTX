// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/transfer.hpp"
#include "ttx/graph/policies/none.hpp"

using namespace Ttx::Graph;
using namespace Ttx::Data;

S32 main() {
  U32 value = 73, output = 0;
  if (Transfer::copy(Form::Storage::from(value), Form::Storage::from(output)) !=
          Status::Success ||
      output != value) {
    return 1;
  }

  const auto none = Policies::None::get_none();
  if (none.get_abstract().get_reachability() !=
      Abstract::Reachability::Unreachable) {
    return 2;
  }

  return none.get_abstract().bind<Abstract>().visit(
      [](Abstract bound) -> S32 {
        const auto next = bound.resolve_route({});
        return next.get_reachability() == Abstract::Reachability::Unreachable
                   ? 0
                   : 3;
      },

      []() -> S32 { return 4; });
}
