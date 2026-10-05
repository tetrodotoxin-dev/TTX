// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/negotiation/library.hpp"

#include "perimortem/core/null_terminated.hpp"

using namespace Ttx::Semantic::Negotiation;
using namespace Perimortem;

auto Library::open(Core::View::Bytes path, Memory::Allocator::Arena& errors)
    -> Utility::Result<Library, Core::View::Bytes> {
  using Result = Utility::Result<Library, Core::View::Bytes>;
  return System::Library::open(path, errors)
      .visit(
          [&](System::Library& library) -> Result {
            return library
                .symbol(
                    Core::NullTerminated::to_view(
                        TTX_LIBRARY_ENTRY, sizeof(TTX_LIBRARY_ENTRY) - 1),
                    errors)
                .visit(
                    [&](void* entry) -> Result {
                      return Library(
                          Core::Data::take(library),
                          reinterpret_cast<ttx_library_entry>(entry));
                    },
                    [](Core::View::Bytes error) -> Result { return error; });
          },
          [](Core::View::Bytes error) -> Result { return error; });
}

auto Library::visit(Query host, Callback callback) const -> Binding::Status {
  const auto status = entry(host, callback.get_abi());
  return status == TTX_BINDING_SATISFIED || status == TTX_BINDING_UNKNOWN
             ? static_cast<Binding::Status>(status)
             : Binding::Status::Rejected;
}
