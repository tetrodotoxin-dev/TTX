// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_PROTOCOL_SHARED_PROVIDER_H
#define TTX_DATA_PROTOCOL_SHARED_PROVIDER_H

#include "ttx/data/form/representation.h"
#include "ttx/data/protocol/shared/lifetime.h"

// A Shared provider lends a payload while retaining the required resource. A
// successful acquisition supplies ready data and transfers one release
// obligation. Acquisition finishes before returning, while its lifetime may
// span several observations. Failure transfers no lifetime.

typedef struct ttx_shared_provider_operations {
  const ttx_representation* (*representation)(void* source);
  // Success supplies ready payload data and transfers its release
  // obligation. Failure supplies no lifetime. Acquisition itself is finished
  // before returning, even though the acquired lifetime continues afterward.
  ttx_data_status (*acquire)(void* source, ttx_shared_lifetime* result);
} ttx_shared_provider_operations;

typedef struct ttx_shared_provider {
  void* source;
  const ttx_shared_provider_operations* operations;
} ttx_shared_provider;

// Describe the callable API independently of the payload it transports.
C_LINKAGE const ttx_representation* ttx_shared_provider_representation(void);

#endif
