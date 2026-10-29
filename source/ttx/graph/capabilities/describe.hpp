// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/capabilities/describe.h"

namespace Ttx::Graph::Capabilities {

// Describe exposes optional UTF 8 metadata through its supplied operations.
// An empty Option means unavailable metadata. A present empty View is an
// intentionally empty value. Returned Views share the validity window in
// describe.h. Construction copies the API record under its supplying access.
class Describe {
 public:
  using Api = ttx_describe;
  static constexpr auto contract_id = Perimortem::Core::Identity::Uuid(
      TTX_DESCRIBE_ID_HIGH,
      TTX_DESCRIBE_ID_LOW);

  explicit constexpr Describe(Api api) : api(api) {}
  constexpr auto get_abi() const -> Api { return api; }

  auto get_name() const
      -> Perimortem::Core::Storage::Option<Perimortem::Core::View::Bytes> {
    perimortem_view_bytes output{};
    if (api.operations->get_name(api.context, &output) == 1) {
      return Perimortem::Core::View::Bytes(output);
    }

    return {};
  }

  auto get_description() const
      -> Perimortem::Core::Storage::Option<Perimortem::Core::View::Bytes> {
    perimortem_view_bytes output{};
    if (api.operations->get_description(api.context, &output) == 1) {
      return Perimortem::Core::View::Bytes(output);
    }

    return {};
  }

 private:
  Api api;
};

}  // namespace Ttx::Graph::Capabilities
