// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef VALIDATION_RETENTION_PROVIDER_H
#define VALIDATION_RETENTION_PROVIDER_H

#include "ttx/concept/capabilities/create.h"

typedef struct retention_statistics {
  Count callbacks;
  Count acquisitions;
  Count releases;
  Count live;
  Count calls;
  U8 inside_callback;
} retention_statistics;

// The fixture exposes observations separately from the created interfaces.
// They remain available after release, when the consumer's access has ended.
typedef struct retention_fixture {
  ttx_abstract factory;
  retention_statistics (*statistics)(void);
  void (*configure)(
      ttx_negotiation_status creation,
      ttx_negotiation_status retention);
} retention_fixture;

C_LINKAGE EXPORTED(TTX_TEST)
retention_fixture retention_provider_open(void);

#endif
