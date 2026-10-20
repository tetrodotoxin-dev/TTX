// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/support/library.hpp"

#include "perimortem/core/data.hpp"

#ifdef TOOLCHAIN_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "perimortem/core/diagnostics/log.hpp"
#include "perimortem/core/storage/vector.hpp"
#include "perimortem/core/view/null_terminated.hpp"
#include "perimortem/memory/thread/bytes.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Perimortem::System;
using namespace Validation::Support;

auto Validation::Support::library_path(View::Bytes name) -> Thread::Bytes {
  Storage::Vector<U8, 4096> path;

#ifdef TOOLCHAIN_WINDOWS

  Storage::Vector<WCHAR, 4096> native;
  const auto length =
      GetModuleFileNameW(nullptr, native.get_data(), native.get_size());
  const auto size = length && length < native.get_size()
                        ? WideCharToMultiByte(
                              CP_UTF8, WC_ERR_INVALID_CHARS, native.get_data(),
                              length, Data::cast<char>(path.get_data()),
                              path.get_size(), nullptr, nullptr)
                        : 0;

#else

  const auto size = readlink(
      "/proc/self/exe", Data::cast<char>(path.get_data()), sizeof(path));

#endif

  if (size <= 0 || Count(size) == sizeof(path)) {
    Diagnostics::Log::fatal("Cannot locate the fixture executable."_view);
  }

  Count end = Count(size);
  while (end && path[end - 1] != '/' && path[end - 1] != '\\') {
    --end;
  }

  Thread::Bytes location(View::Bytes(path[0], end));
  location.concat("validation/providers/"_view);
  location.concat(name);
  return location;
}

auto Validation::Support::open_library(View::Bytes name) -> Process::Library {
  const auto location = library_path(name);
  Allocator::Arena errors;
  return Process::Library::open(location.get_view(), errors)
      .visit(
          [](Process::Library& library) { return Data::take(library); },
          [](View::Bytes message) -> Process::Library {
            Diagnostics::Log::fatal(message);
          });
}

auto Validation::Support::find_symbol(
    const Process::Library& library,
    View::Bytes name) -> void* {
  Allocator::Arena errors;
  return library.symbol(name, errors)
      .visit(
          [](void* symbol) { return symbol; },
          [](View::Bytes message) -> void* {
            Diagnostics::Log::fatal(message);
          });
}
