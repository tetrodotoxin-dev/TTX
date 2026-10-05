// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/providers/invocation/provider.h"

#include <stddef.h>
#include <string.h>

#include "ttx/data/protocol/block/provider.h"
#include "ttx/data/protocol/direct/provider.h"
#include "ttx/data/protocol/fragment/provider.h"
#include "ttx/data/protocol/shared/provider.h"
#include "ttx/semantic/transport/flow.h"

static struct {
  invocation_statistics statistics;
  ttx_representation record;
  ttx_representation inputs;
  ttx_representation outputs;
  U8 bytes[3][512];
  U8 protocol;
  U8 fail_transfer;
  ttx_binding_status status;
  ttx_invocation invocation;
  R64 bias;
} state;

static ttx_data_status invoke(void* context, const void* input, void* output) {
  R64* source = context;
  const invocation_input* values = input;
  const R64 bias = *source;
  ++state.statistics.calls;
  R64 result = (values->value + bias) * values->scale;
  *(R64*)output = values->negate ? -result : result;
  return TTX_DATA_SUCCESS;
}

static const ttx_representation* representation(void* context) {
  (void)context;
  return &state.record;
}

static const void* pointer(void* context) {
  (void)context;
  return &state.invocation;
}

static ttx_data_status commit(void* context, ttx_storage target) {
  R64* source = context;
  ++state.statistics.transfers;
  if (state.fail_transfer) {
    return TTX_DATA_IO_ERROR;
  }

  // Block creates the bridge in the caller's record. It does not need an
  // independently allocated table for this interior context.
  const ttx_invocation value = {
    source,
    &state.inputs,
    &state.outputs,
    invoke,
  };
  memcpy(target.data, &value, sizeof(value));
  return TTX_DATA_SUCCESS;
}

static ttx_data_status
    get_pointer(void* context, Count position, void** output) {
  R64* source = context;
  ++state.statistics.transfers;
  if (position == 0) {
    *output = source;
    return TTX_DATA_SUCCESS;
  }

  if (state.fail_transfer) {
    return TTX_DATA_IO_ERROR;
  }

  if (position == offsetof(ttx_invocation, inputs)) {
    *output = &state.inputs;
    return TTX_DATA_SUCCESS;
  }

  if (position == offsetof(ttx_invocation, outputs)) {
    *output = &state.outputs;
    return TTX_DATA_SUCCESS;
  }

  // The agreed System V carrier contains the executable pointer's bits.
  // Supply that slot separately instead of allocating another combined record.
  ttx_data_status (*operation)(void*, const void*, void*) = invoke;
  memcpy(output, &operation, sizeof(operation));
  return TTX_DATA_SUCCESS;
}

static void release(void* context) {
  (void)context;
  ++state.statistics.releases;
}

static ttx_data_status acquire(void* context, ttx_shared_lifetime* output) {
  (void)context;
  ++state.statistics.acquisitions;
  *output = (ttx_shared_lifetime){
    &state.invocation,
    NULL,
    release,
  };
  return TTX_DATA_SUCCESS;
}

static ttx_binding_status
    provider(void* context, perimortem_uuid id, ttx_storage requested) {
  R64* source = context;
  ++state.statistics.binds;
  if (state.status != TTX_BINDING_SATISFIED) {
    return state.status;
  }

  if (state.protocol == 0 && id.high == TTX_DIRECT_PROVIDER_ID_HIGH &&
      id.low == TTX_DIRECT_PROVIDER_ID_LOW) {
    static const ttx_direct_provider_operations operations = {
      representation,
      pointer,
    };
    const ttx_direct_provider api = {
      source,
      &operations,
    };
    return ttx_binding_provide(
        ttx_direct_provider_representation(), &api, requested);
  }

  if (state.protocol == 1 && id.high == TTX_SHARED_PROVIDER_ID_HIGH &&
      id.low == TTX_SHARED_PROVIDER_ID_LOW) {
    static const ttx_shared_provider_operations operations = {
      representation,
      acquire,
    };
    const ttx_shared_provider api = {
      source,
      &operations,
    };
    return ttx_binding_provide(
        ttx_shared_provider_representation(), &api, requested);
  }

  if (state.protocol == 2 && id.high == TTX_BLOCK_PROVIDER_ID_HIGH &&
      id.low == TTX_BLOCK_PROVIDER_ID_LOW) {
    static const ttx_block_provider_operations operations = {
      representation,
      commit,
    };
    const ttx_block_provider api = {
      source,
      &operations,
    };
    return ttx_binding_provide(
        ttx_block_provider_representation(), &api, requested);
  }

  if (state.protocol == 3 && id.high == TTX_FRAGMENT_PROVIDER_ID_HIGH &&
      id.low == TTX_FRAGMENT_PROVIDER_ID_LOW) {
    static const ttx_fragment_provider_operations operations = {
      .representation = representation,
      .get_pointer = get_pointer,
    };
    const ttx_fragment_provider api = {
      source,
      &operations,
    };
    return ttx_binding_provide(
        ttx_fragment_provider_representation(), &api, requested);
  }

  return TTX_BINDING_UNKNOWN;
}

static invocation_statistics statistics(void) {
  return state.statistics;
}

static ttx_binding_status supports(void* context, perimortem_uuid id) {
  (void)context;
  if (state.status != TTX_BINDING_SATISFIED) {
    return state.status;
  }

  const perimortem_uuid contracts[] = {
    {
      TTX_DIRECT_PROVIDER_ID_HIGH,
      TTX_DIRECT_PROVIDER_ID_LOW,
    },
    {
      TTX_SHARED_PROVIDER_ID_HIGH,
      TTX_SHARED_PROVIDER_ID_LOW,
    },
    {
      TTX_BLOCK_PROVIDER_ID_HIGH,
      TTX_BLOCK_PROVIDER_ID_LOW,
    },
    {
      TTX_FRAGMENT_PROVIDER_ID_HIGH,
      TTX_FRAGMENT_PROVIDER_ID_LOW,
    },
  };
  return state.protocol < 4 && id.high == contracts[state.protocol].high &&
                 id.low == contracts[state.protocol].low
             ? TTX_BINDING_SATISFIED
             : TTX_BINDING_UNKNOWN;
}

static void
    configure(U8 protocol, ttx_binding_status status, U8 fail_transfer) {
  state.protocol = protocol;
  state.status = status;
  state.fail_transfer = fail_transfer;
  state.statistics = (invocation_statistics){
    0,
  };
}

invocation_fixture invocation_provider_open(
    const ttx_representation* record,
    const ttx_representation* inputs,
    const ttx_representation* outputs) {
  const ttx_representation* sources[] = {
    record,
    inputs,
    outputs,
  };
  ttx_representation* targets[] = {
    &state.record,
    &state.inputs,
    &state.outputs,
  };
  for (U32 index = 0; index < 3; ++index) {
    if (sources[index]->size > sizeof(state.bytes[index])) {
      return (invocation_fixture){
        0,
      };
    }

    memcpy(state.bytes[index], sources[index]->data, sources[index]->size);
    *targets[index] = (ttx_representation){
      state.bytes[index],
      sources[index]->size,
    };
  }

  state.bias = 2.0;
  state.invocation = (ttx_invocation){
    &state.bias,
    &state.inputs,
    &state.outputs,
    invoke,
  };
  configure(0, TTX_BINDING_SATISFIED, 0);
  return (invocation_fixture){
    {
      &state.bias,
      provider,
      supports,
    },
    statistics,
    configure,
  };
}
