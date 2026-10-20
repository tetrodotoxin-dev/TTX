// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include <stdio.h>
#include <stdlib.h>

#include "toolchain/validation/test.hpp"

#include "perimortem/core/view/null_terminated.hpp"
#include "perimortem/memory/thread/vector.hpp"

#include "ttx/data/form/representation.hpp"
#include "ttx/data/form/schema.hpp"

namespace Validation::DataTests {

// A test owns its prepared metadata for exactly the duration of that case.
// Keeping preparation outside operation checks lets those checks detect
// accidental compilation or allocation during established transfer use.
class Preparation {
 public:
  auto operator()(Ttx::Data::Form::Schema::Reference source)
      -> Ttx::Data::Form::Representation {
    return Ttx::Data::Form::Representation::compile(source).visit(
        [&](Perimortem::Memory::Global::Bytes& bytes)
            -> Ttx::Data::Form::Representation {
          const auto& stored =
              buffers.insert(Perimortem::Core::Data::take(bytes));
          return {stored.get_data(), stored.get_size()};
        },

        [](Ttx::Data::Status status) -> Ttx::Data::Form::Representation {
          fputs("Invalid fixture Representation.", stderr);
          abort();
        });
  }

  auto validate(Ttx::Data::Form::Schema::Reference source)
      -> Ttx::Data::Status {
    return Ttx::Data::Form::Representation::compile(source).visit(
        [](const Perimortem::Memory::Global::Bytes& bytes) {
          return Ttx::Data::Status::Success;
        },

        [](Ttx::Data::Status status) { return status; });
  }

 private:
  Perimortem::Memory::Thread::Vector<Perimortem::Memory::Global::Bytes> buffers;
};

}  // namespace Validation::DataTests
