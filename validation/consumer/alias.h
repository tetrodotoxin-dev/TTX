// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_VALIDATION_CONSUMER_ALIAS_H
#define TTX_VALIDATION_CONSUMER_ALIAS_H

#include "ttx/graph/abstract.h"

C_LINKAGE S32 validate_alias(ttx_abstract location);
C_LINKAGE S32
    validate_c_provider(ttx_abstract initial, ttx_abstract replacement);
C_LINKAGE S32 consume_c_provider(
    ttx_abstract location,
    ttx_abstract initial,
    ttx_abstract replacement);

#endif
