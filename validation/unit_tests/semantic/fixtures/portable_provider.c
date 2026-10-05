// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/semantic/fixtures/portable_provider.h"

static U32 value = 40;
static U32 calls;

static U32 add(void* context, U32 amount) {
  U32* source = context;
  ++calls;
  return *source + amount;
}

// Root fields are the opaque context and a pointer to add. The callable
// takes that context and one U32, and returns U32. Wasm uses four byte slots
// and convention three. Native64 uses eight byte slots and convention one.
#if defined(__EMSCRIPTEN__)
static const U8 bytes[] = {
  0x10, 0,    0, 0, 1, 0, 0, 0, 0x21, 0x40, 0x80, 0, 0x80, 0x10, 0, 1,
  0x83, 0x11, 4, 1, 3, 0, 3, 2, 0x80, 0,    0,    1, 3,    0,    0, 1,
};
#else
static const U8 bytes[] = {
  0x21, 0x80, 0, 1, 0x80, 0x20, 0, 1, 0x83, 0x21, 8, 1,
  3,    0,    1, 2, 0x80, 0,    0, 1, 3,    0,    0, 1,
};
#endif
static const ttx_representation form = {
  bytes,
  sizeof(bytes),
};
static ttx_binding_status
    bind(void* context, perimortem_uuid id, ttx_storage output) {
  U32* source = context;
  if (id.high != 17 || id.low != 23) {
    return TTX_BINDING_UNKNOWN;
  }

  const portable_counter api = {
    source,
    add,
  };
  return ttx_binding_provide(&form, &api, output);
}

static ttx_binding_status supports(void* context, perimortem_uuid id) {
  (void)context;
  return id.high == 17 && id.low == 23 ? TTX_BINDING_SATISFIED
                                       : TTX_BINDING_UNKNOWN;
}

ttx_semantic_query portable_counter_open(void) {
  return (ttx_semantic_query){
    &value,
    bind,
    supports,
  };
}

U32 portable_counter_calls(void) {
  return calls;
}
