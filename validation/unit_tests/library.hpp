// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/system/library.hpp"

namespace Validation {

// Fixtures are built beside the runner. Keep the Library alive
// through every borrowed thunk, and report fixture failures at their source.
auto open_library(Perimortem::Core::View::Bytes name)
    -> Perimortem::System::Library;
auto find_symbol(
    const Perimortem::System::Library& library,
    Perimortem::Core::View::Bytes name) -> void*;

}  // namespace Validation
