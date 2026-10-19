// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_POLICIES_BORROWED_H
#define TTX_CONCEPT_POLICIES_BORROWED_H

#include "ttx/concept/abstract.h"

#define TTX_BORROWED_ID_HIGH 0xcd1fc9de2b8a4663ULL
#define TTX_BORROWED_ID_LOW 0x890b83ffde67368bULL

// Borrowed provides acquired access to an Abstract until the consumer calls
// release. The provider keeps the context, operations and referenced metadata
// accessible through that acquisition. Observations can change under the
// governing policies while access remains valid.
//
// Each successful Borrow creates one release obligation, including when two
// acquisitions use the same context. Copying or binding through an acquisition
// shares its existing access and obligation. The consumer releases each
// acquisition exactly once after using it. Release ends access through that
// acquisition and its copies. Using them afterward is undefined behavior.
//
// An operation transferring retained access returns Borrowed or a stronger
// contract that exposes its release obligation. A plain Abstract result lets
// its consumer finish without releasing it.
typedef struct ttx_borrowed {
  void* context;
  const struct ttx_borrowed_ops* operations;
} ttx_borrowed;

typedef struct ttx_borrowed_ops {
  ttx_abstract_ops abstract;
  void (*release)(void* context);
} ttx_borrowed_ops;

C_LINKAGE EXPORTED(TTX) const ttx_representation* ttx_borrowed_representation(
    void);

#endif
