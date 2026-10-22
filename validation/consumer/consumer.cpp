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
  if (none.get_abstract().get_state() != Abstract::State::None) {
    return 2;
  }

  return none.get_abstract().bind<Policies::None>().visit(
      [](Policies::None bound) -> S32 {
        const auto next = bound.get_abstract().resolve_route({});
        return next.get_state() == Abstract::State::None ? 0 : 3;
      },

      []() -> S32 { return 4; });
}
