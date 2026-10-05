// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_POLICIES_BORROWED_H
#define TTX_CONCEPT_POLICIES_BORROWED_H
#include "ttx/concept/abstract.h"

#define TTX_BORROWED_ID_HIGH 0xcd1fc9de2b8a4663ULL
#define TTX_BORROWED_ID_LOW 0x890b83ffde67368bULL

// Borrowed exposes an Abstract whose acquired access lasts until release. An
// operation transferring retained access returns this contract and passes
// responsibility for the release to its caller. The provider keeps the
// answer's data and policy available until that release.
//
// Binding or copying a view of the answer shares the existing access and
// release obligation. Borrow requests another acquired answer, with its own
// release, even when the provider returns the same context again.
//
// Operations that require the caller to release their result must return
// Borrowed or a stronger contract that requires it. Returning that result only
// as Abstract hides the release obligation and can leak the retained
// resources.
typedef struct ttx_borrowed {
  void* context;
  const struct ttx_borrowed_ops* operations;
} ttx_borrowed;

typedef struct ttx_borrowed_ops {
  ttx_abstract_ops abstract;
  void (*release)(void* context);
} ttx_borrowed_ops;

C_LINKAGE const ttx_representation* ttx_borrowed_representation(void);
#endif
