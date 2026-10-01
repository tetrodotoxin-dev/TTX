// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_PROTOCOL_FRAGMENT_PROVIDER_H
#define TTX_DATA_PROTOCOL_FRAGMENT_PROVIDER_H

#include "ttx/data/form/representation.h"

// Fragment produces individual values through typed synchronous getters at
// agreed byte coordinates. Its representation determines which getters are
// present. Each output belongs to the call and cannot be retained for later
// work. Results are native values even when the payload uses another byte
// order, leaving realization into destination bytes with the caller.
//
// Successive reads need not form a snapshot. An overlapping destination can
// affect later observations. Pointer results describe their storage without
// promising that a foreign pointer can be used as a native address.

typedef struct ttx_fragment_provider_operations {
  const ttx_representation* (*representation)(void* source);
  ttx_data_status (*get_u8)(void* source, Count position, U8* result);
  ttx_data_status (*get_u16)(void* source, Count position, U16* result);
  ttx_data_status (*get_u32)(void* source, Count position, U32* result);
  ttx_data_status (*get_u64)(void* source, Count position, U64* result);
  ttx_data_status (*get_s8)(void* source, Count position, S8* result);
  ttx_data_status (*get_s16)(void* source, Count position, S16* result);
  ttx_data_status (*get_s32)(void* source, Count position, S32* result);
  ttx_data_status (*get_s64)(void* source, Count position, S64* result);
  ttx_data_status (*get_r32)(void* source, Count position, R32* result);
  ttx_data_status (*get_r64)(void* source, Count position, R64* result);
  ttx_data_status (*get_pointer)(void* source, Count position, void** result);
  ttx_data_status (
      *get_v64)(void* source, Count position, ttx_vector64* result);
  ttx_data_status (
      *get_v128)(void* source, Count position, ttx_vector128* result);
  ttx_data_status (
      *get_v256)(void* source, Count position, ttx_vector256* result);
  ttx_data_status (
      *get_v512)(void* source, Count position, ttx_vector512* result);
} ttx_fragment_provider_operations;

typedef struct ttx_fragment_provider {
  void* source;
  const ttx_fragment_provider_operations* operations;
} ttx_fragment_provider;

// Describe the callable API independently of the payload it transports.
C_LINKAGE const ttx_representation* ttx_fragment_provider_representation(void);

#endif
