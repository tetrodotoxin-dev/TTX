// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/transfer.hpp"

#if __has_include(                           \
    "ttx/semantic/negotiation/query.hpp") || \
    __has_include("ttx/concept/abstract.hpp")
#error The Data component exposed a higher layer.
#endif

using namespace Ttx::Data;

S32 main() {
  U32 input = 73, output = 0;
  if (Transfer::copy(Form::Storage::from(input), Form::Storage::from(output)) !=
      Status::Success) {
    return 1;
  }

  const Bool compiled =
      Form::Representation::compile(Form::Native<U32>::reference)
          .visit(
              [](const Form::Representation& form) {
                return form.compatible(
                    Form::Compiled<
                        Form::Native<U32>::reference>::get_representation());
              },

              [](Status status) { return False; });
  return compiled && output == input ? 0 : 2;
}
