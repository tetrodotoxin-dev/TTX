// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/system/library.hpp"

namespace Validation {

// Provider libraries live in validation/providers relative to the runner.
// Keep each Library alive through every borrowed operation and report fixture
// failures at their source.
auto open_library(Perimortem::Core::View::Bytes name)
    -> Perimortem::System::Library;
auto find_symbol(
    const Perimortem::System::Library& library,
    Perimortem::Core::View::Bytes name) -> void*;

}  // namespace Validation
