// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/memory/thread/bytes.hpp"
#include "perimortem/system/process/library.hpp"

namespace Validation::Support {

// Provider libraries live in validation/providers relative to the runner.
// Keep each Library alive through every borrowed operation and report fixture
// failures at their source.
auto library_path(Perimortem::Core::View::Bytes name)
    -> Perimortem::Memory::Thread::Bytes;
auto open_library(Perimortem::Core::View::Bytes name)
    -> Perimortem::System::Process::Library;
auto find_symbol(
    const Perimortem::System::Process::Library& library,
    Perimortem::Core::View::Bytes name) -> void*;

}  // namespace Validation::Support
