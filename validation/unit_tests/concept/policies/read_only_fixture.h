// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once
#include "ttx/concept/abstract.h"

#define TEST_READ_ID_HIGH 0x9f7be2929fa54679ULL
#define TEST_READ_ID_LOW 0xa7f8f4a4b03a5823ULL

typedef struct test_read_ops {
  ttx_abstract_ops abstract;
  U32 (*value)(void* source);
} test_read_ops;
typedef struct test_read {
  void* source;
  const test_read_ops* operations;
} test_read;

// The C provider publishes a heap value during the callback. Borrow can retain
// it after that observation. freed is supplied by the test and outlives all
// views.
PERIMORTEM_C void test_read_only_open(
    const ttx_representation* read,
    U32* freed,
    void* receiver,
    void (*receive)(void*, ttx_abstract));
