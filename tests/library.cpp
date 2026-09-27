// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tests/library.hpp"

#include <unistd.h>

#include "perimortem/core/static/vector.hpp"
#include "perimortem/core/diagnostics/log.hpp"
#include "perimortem/core/null_terminated.hpp"

#include "perimortem/memory/dynamic/bytes.hpp"

using namespace Perimortem;
using namespace Perimortem::Core;

auto Validation::open_library(View::Bytes name) -> System::Library {
  Static::Vector<U8, 4096> path;
  const auto size = readlink(
      "/proc/self/exe", reinterpret_cast<char*>(path.get_data()), sizeof(path));
  if (size <= 0 || Count(size) == sizeof(path)) {
    Diagnostics::Log::fatal("Cannot locate the fixture executable."_view);
  }

  Count end = Count(size);
  while (end && path[end - 1] != '/') {
    --end;
  }
  Memory::Dynamic::Bytes location(View::Bytes(path.get_data(), end));
  location.concat(name);
  Memory::Allocator::Arena errors;
  return System::Library::open(location.get_view(), errors)
      .visit(
          [](System::Library& library) { return Data::take(library); },
          [](View::Bytes message) -> System::Library {
            Diagnostics::Log::fatal(message);
          });
}

auto Validation::find_symbol(const System::Library& library, View::Bytes name)
    -> void* {
  Memory::Allocator::Arena errors;
  return library.symbol(name, errors)
      .visit(
          [](void* symbol) { return symbol; },
          [](View::Bytes message) -> void* {
            Diagnostics::Log::fatal(message);
          });
}
