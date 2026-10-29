// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include <stdio.h>
#include <stdlib.h>

#include "toolchain/validation/test.hpp"

#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/data/form/representation.hpp"
#include "ttx/data/form/schema.hpp"

namespace Validation::DataTests {

// A test owns its prepared metadata for exactly the duration of that case.
// Keeping preparation outside operation checks lets those checks detect
// accidental compilation or allocation during established transfer use.
class Preparation {
 public:
  // Pointer width is policy for every later preparation. Explicit construction
  // prevents an unrelated integer fixture from silently selecting that ABI.
  explicit Preparation(Count pointer_size = sizeof(void*))
      : pointer_size(pointer_size) {}

  auto operator()(Ttx::Data::Form::Schema::Reference source)
      -> Ttx::Data::Form::Representation {
    return Ttx::Data::Form::Representation::compile(source, pointer_size)
        .visit(
            [](Ttx::Data::Form::Representation& representation)
                -> Ttx::Data::Form::Representation {
              return Perimortem::Core::Data::take(representation);
            },

            [](Ttx::Data::Status status) -> Ttx::Data::Form::Representation {
              fputs("Invalid fixture Representation.", stderr);
              abort();
            });
  }

  auto validate(Ttx::Data::Form::Schema::Reference source)
      -> Ttx::Data::Status {
    return Ttx::Data::Form::Representation::compile(source, pointer_size)
        .visit(
            [](const Ttx::Data::Form::Representation& representation) {
              return Ttx::Data::Status::Success;
            },

            [](Ttx::Data::Status status) { return status; });
  }

 private:
  Count pointer_size;
};

}  // namespace Validation::DataTests
