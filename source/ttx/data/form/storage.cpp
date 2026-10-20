// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#include "ttx/data/form/storage.hpp"

using namespace Ttx::Data;

auto ttx_storage_check(ttx_storage storage) -> ttx_data_status {
  if (!storage.representation) {
    return TTX_DATA_INVALID;
  }

  const Form::Representation representation(*storage.representation);
  if (storage.size < representation.get_extent()) {
    return TTX_DATA_BOUNDS;
  }

  if (representation.get_extent() && !storage.data) {
    return TTX_DATA_INVALID;
  }

  if (!__builtin_is_aligned(storage.data, representation.get_alignment())) {
    return TTX_DATA_INVALID;
  }

  return TTX_DATA_SUCCESS;
}
