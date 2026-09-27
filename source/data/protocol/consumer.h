// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_PROTOCOL_CONSUMER_H
#define TTX_DATA_PROTOCOL_CONSUMER_H

#include "ttx/data/form/representation.h"

// Consumer supplies the representation it accepts. A semantic contract selects
// how the provider may supply that data, so several contracts can use this
// same API without implying acceptance of one another. The owner retains its
// requirement through negotiation and use. Individual operations supply their
// own destination Storage after agreement and keep it alive through the call.

typedef struct ttx_consumer_operations {
  const ttx_representation* (*representation)(const void* source);
} ttx_consumer_operations;

typedef struct ttx_consumer {
  const void* source;
  const ttx_consumer_operations* operations;
} ttx_consumer;

// Describe the callable API independently of the payload it transports.
PERIMORTEM_C const ttx_representation* ttx_consumer_representation(void);

#endif
