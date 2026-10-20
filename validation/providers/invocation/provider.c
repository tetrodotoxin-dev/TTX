// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/providers/invocation/provider.h"

#include <string.h>

#include "ttx/data/transfer.h"

static struct {
  invocation_statistics statistics;
  ttx_representation record, inputs, outputs;
  U8 bytes[3][512];
  U8 malformed;
  ttx_negotiation_status status;
  R64 bias;
} state;

// The input contains a value, scale and negate flag, while context points to
// the provider's bias. Each call combines them into an R64 result in output.
// The consumer receives that computed result by calling the supplied function.
static ttx_data_status invoke(void* context, const void* input, void* output) {
  const R64* bias = context;
  const invocation_input* values = input;
  R64* destination = output;
  ++state.statistics.calls;

  const R64 value = (values->value + *bias) * values->scale;
  *destination = values->negate ? -value : value;
  return TTX_DATA_SUCCESS;
}

static ttx_negotiation_status supports(void* context, perimortem_uuid id) {
  (void)context;
  if (id.high != INVOCATION_METHOD_HIGH || id.low != INVOCATION_METHOD_LOW) {
    return TTX_NEGOTIATION_UNKNOWN;
  }

  return state.status;
}

static ttx_negotiation_status
    bind(void* context, perimortem_uuid id, ttx_storage target) {
  ++state.statistics.binds;

  const ttx_negotiation_status status = supports(context, id);
  if (status != TTX_NEGOTIATION_SATISFIED) {
    return status;
  }

  ttx_invocation api = {
    &state.bias, &state.inputs, &state.outputs,
    state.malformed ? NULL : invoke};

  const ttx_storage input = {&state.record, (U8*)&api, sizeof(api)};
  const ttx_data_status transferred = ttx_transfer(&input, &target);
  if (transferred != TTX_DATA_SUCCESS) {
    return TTX_NEGOTIATION_REJECTED;
  }

  ++state.statistics.transfers;
  return TTX_NEGOTIATION_SATISFIED;
}

static invocation_statistics statistics(void) {
  return state.statistics;
}

static void configure(ttx_negotiation_status status, U8 malformed) {
  state.status = status;
  state.malformed = malformed;
  state.statistics = (invocation_statistics){0};
}

invocation_fixture invocation_provider_open(
    const ttx_representation* record,
    const ttx_representation* inputs,
    const ttx_representation* outputs) {
  const ttx_representation* sources[] = {record, inputs, outputs};
  ttx_representation* targets[] = {
    &state.record, &state.inputs, &state.outputs};

  for (U32 index = 0; index != 3; ++index) {
    if (sources[index]->size > sizeof(state.bytes[index])) {
      return (invocation_fixture){0};
    }

    memcpy(state.bytes[index], sources[index]->data, sources[index]->size);
    *targets[index] =
        (ttx_representation){state.bytes[index], sources[index]->size};
  }

  state.bias = 2.0;
  configure(TTX_NEGOTIATION_SATISFIED, 0);
  return (invocation_fixture){{&state, bind, supports}, statistics, configure};
}
