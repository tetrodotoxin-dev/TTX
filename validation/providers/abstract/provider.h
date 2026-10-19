// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef VALIDATION_BOOTSTRAP_PROVIDER_H
#define VALIDATION_BOOTSTRAP_PROVIDER_H

#include "ttx/concept/abstract.h"

#define BOOTSTRAP_EXPORTS_HIGH 0x3d46d55e985b4c16ULL
#define BOOTSTRAP_EXPORTS_LOW 0x86ee85a33be1458aULL
#define BOOTSTRAP_QUERY_HIGH 0x10d12c0061b54077ULL
#define BOOTSTRAP_QUERY_LOW 0xb131bd52aa477f61ULL

// Two callable representations of the fixture's single-root export inventory.
typedef struct bootstrap_exports {
  void* context;
  void (*visit)(
      void* context,
      void* callback_context,
      void (*callback)(void*, ttx_abstract));
} bootstrap_exports;

typedef struct bootstrap_indexed {
  void* context;
  U64 (*count)(void* context);
  ttx_abstract (*get)(void* context, U64 index);
} bootstrap_indexed;

#endif
