// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/memory/dynamic/record.hpp"

#include "perimortem/system/library.hpp"

#include "ttx/semantic/negotiation/callback.hpp"
#include "ttx/semantic/negotiation/library.h"

namespace Ttx::Semantic::Negotiation {

// Loads a DLL or SO through System::Library and finds its `ttx_query` entry.
// `visit` calls that entry with the host's Query and the caller's callback.
// Copies keep the same library loaded. Release retained provider data before
// destroying the final copy so its release functions can still execute.
class Library {
 public:
  static auto open(
      Perimortem::Core::View::Bytes path,
      Perimortem::Memory::Allocator::Arena& errors)
      -> Perimortem::Utility::Result<Library, Perimortem::Core::View::Bytes>;
  Library(const Library&) = default;
  auto operator=(const Library&) -> Library& = default;
  Library(Library&&) = default;

  auto visit(Query host, Callback callback) const -> Binding::Status;

 private:
  Library(Perimortem::System::Library library, ttx_library_entry entry)
      : library(Perimortem::Core::Data::take(library)), entry(entry) {}

  Perimortem::Memory::Dynamic::Record<Perimortem::System::Library> library;
  ttx_library_entry entry;
};

}  // namespace Ttx::Semantic::Negotiation
