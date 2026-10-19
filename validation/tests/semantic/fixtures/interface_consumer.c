// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/providers/interface/provider.h"

U64 counter_consume(counter_api api, U64 amount) {
  api.add(api.context, amount);
  return api.read(api.context);
}
