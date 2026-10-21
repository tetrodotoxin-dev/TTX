// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/transfer.hpp"
#include "ttx/graph/policies/none.hpp"

using namespace Ttx::Graph;
using namespace Ttx::Data;

S32 main() {
  U32 value = 73, output = 0;
  if (Transfer::copy(Form::Storage::from(value), Form::Storage::from(output)) !=
      Status::Success) {
    return 1;
  }

  const auto none = Policies::None::get_none();
  if (none.get_abstract().get_state() != Abstract::State::None) {
    return 2;
  }

  if (none.get_abstract().get_state() != Abstract::State::None) {
    return 3;
  }

  return output == 73 ? 0 : 4;
}
