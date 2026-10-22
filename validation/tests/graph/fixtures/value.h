// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef VALIDATION_GRAPH_VALUE_H
#define VALIDATION_GRAPH_VALUE_H

#include "toolchain/toolchain.h"

#define VALUE_ID_HIGH 0xa883cd2142ef4478ULL
#define VALUE_ID_LOW 0x9e76594e1b06eb12ULL

// This fixture contract reads one current byte by value. The fixture has no
// Graph operations or Data description. Providers can compute the byte or read
// it from their own state while the supplying contract keeps them available.
typedef struct value_api {
  void* context;
  U8 (*read)(void* context);
} value_api;

#endif
