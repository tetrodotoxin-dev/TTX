// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_CAPABILITIES_BORROW_H
#define TTX_CONCEPT_CAPABILITIES_BORROW_H
#include "ttx/concept/policies/borrowed.h"

#define TTX_BORROW_ID_HIGH 0xd576263608ac45a2ULL
#define TTX_BORROW_ID_LOW 0x9e605b7d64c8a413ULL

// Exposes the capability to keep an Abstract available beyond the observation
// that supplied it. The provider can retain the current object or supply
// another object that preserves the encountered policy.
//
// When access can be retained, `borrow` writes a Borrowed answer to output and
// returns Satisfied. The caller then owes one release through that answer.
// Each successful call creates a separate release obligation, even when the
// provider returns the same context for several requests.
//
// Unknown leaves the borrowing request undetermined. Rejected explicitly
// refuses it. The caller consumes output only when `borrow` returns Satisfied.
typedef struct ttx_borrow {
  void* context;
  const struct ttx_borrow_ops* operations;
} ttx_borrow;

typedef struct ttx_borrow_ops {
  ttx_abstract_ops abstract;
  ttx_binding_status (*borrow)(void* context, ttx_borrowed* output);
} ttx_borrow_ops;

C_LINKAGE const ttx_representation* ttx_borrow_representation(void);
#endif
