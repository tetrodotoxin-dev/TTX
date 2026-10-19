// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/none.hpp"
#include "ttx/data/transfer.hpp"

int main() {
  U32 value = 73, output = 0;
  if (Ttx::Data::Transfer::copy(
          Ttx::Data::Form::Storage::from(value),
          Ttx::Data::Form::Storage::from(output)) !=
      Ttx::Data::Status::Success) {
    return 1;
  }

  const auto none = Ttx::Concept::Policies::None::get_none();
  if (!Ttx::Concept::Abstract::accept(none.get_abstract().get_abi())) {
    return 2;
  }

  return output == 73 ? 0 : 3;
}
