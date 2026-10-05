// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/providers/interface/provider.h"

#include <stddef.h>

#include "validation/providers/representation.h"

typedef struct counter_state {
  U64 guard;
  U64 value;
  U64 second;
  counter_statistics statistics;
  ttx_binding_status status;
  U8 omit;
  U8 stateless;
  enum counter_composition mode;
  U64 selections;
} counter_state;

static counter_state state;
static const ttx_representation* representation;

static U64 add(void* context, U64 amount) {
  U64* source = context;
  ++state.statistics.calls;
  if (!source) {
    return amount;
  }

  U64* value = source;
  *value += amount;
  return *value;
}

static U64 read(void* context) {
  U64* source = context;
  ++state.statistics.calls;
  return source ? *source : 0;
}

// The wrapper owns the relationship between the enclosing state and the
// counter word. The lower operation only receives its own expected context.
static U64 embedded_add(void* context, U64 amount) {
  counter_state* subject = context;
  return add(&subject->value, amount);
}

static U64 embedded_read(void* context) {
  counter_state* subject = context;
  return read(&subject->value);
}

// The temporary value is a simulacra for the lower counter operation, with
// the offset already applied. The operation consumes it during the call, and
// only the numeric result reaches the consumer.
static U64 temporary_add(void* context, U64 amount) {
  counter_state* subject = context;
  U64 temporary = subject->value + 100;
  const U64 result = add(&temporary, amount);
  subject->value = temporary - 100;
  return result;
}

static U64 temporary_read(void* context) {
  counter_state* subject = context;
  U64 temporary = subject->value + 100;
  return read(&temporary);
}

static ttx_binding_status
    bind(void* context, perimortem_uuid id, ttx_storage requested) {
  (void)context;
  ++state.statistics.queries;
  if (state.status != TTX_BINDING_SATISFIED) {
    return state.status;
  }

  if (id.high != COUNTER_ID_HIGH || id.low != COUNTER_ID_LOW) {
    return TTX_BINDING_UNKNOWN;
  }

  if (state.omit) {
    return TTX_BINDING_SATISFIED;
  }

  counter_api api = {state.stateless ? NULL : &state.value, add, read};
  enum counter_composition mode = state.mode;
  if (mode == COUNTER_ALTERNATING) {
    mode = state.selections++ % 2 ? COUNTER_TEMPORARY : COUNTER_DIRECT;
  }

  if (mode == COUNTER_SECOND) {
    api.context = &state.second;
  } else if (mode == COUNTER_EMBEDDED) {
    api = (counter_api){&state, embedded_add, embedded_read};
  } else if (mode == COUNTER_TEMPORARY) {
    api = (counter_api){&state, temporary_add, temporary_read};
  }

  return ttx_binding_provide(representation, &api, requested);
}

static void reset(ttx_binding_status status, U8 omit, U8 stateless) {
  state.statistics = (counter_statistics){
    0,
  };
  state.value = 0;
  state.second = 20;
  state.status = status;
  state.omit = omit;
  state.stateless = stateless;
  state.mode = COUNTER_DIRECT;
  state.selections = 0;
}

// Support observes the semantic promise without constructing the counter API.
// A caller can keep using this fact even if its callable layout has drifted.
static ttx_binding_status supports(void* context, perimortem_uuid id) {
  (void)context;
  if (state.status != TTX_BINDING_SATISFIED) {
    return state.status;
  }

  return id.high == COUNTER_ID_HIGH && id.low == COUNTER_ID_LOW
             ? TTX_BINDING_SATISFIED
             : TTX_BINDING_UNKNOWN;
}

static void compose(enum counter_composition mode) {
  state.mode = mode;
  state.selections = 0;
}

static counter_statistics statistics(void) {
  return state.statistics;
}

counter_fixture interface_provider_open(interface_compile compiler) {
  // These are independently authored C declarations. The host supplies only
  // the compiler, not the expected interface descriptor or native operation
  // identities. sizeof/offsetof retain the actual C record's geometry.
  static const ttx_schema integer = {
    8,
    8,
    TTX_SCHEMA_VALUE,
    {
      .value =
          {
            TTX_SCHEMA_U64,
            TTX_SCHEMA_LITTLE_ENDIAN,
          },
    },
  };
  static const ttx_schema_argument arguments[] = {
    {
      {
        NULL,
        TTX_SCHEMA_REFERENCE_POINTER,
      },
      1,
    },
    {
      {
        &integer,
        0,
      },
      1,
    },
  };
  static const ttx_schema addition = {
    8,
    8,
    TTX_SCHEMA_CALLABLE,
    {
      .callable =
          {
            arguments,
            2,
            {
              &integer,
              0,
            },
            TTX_SCHEMA_SYSTEM_V_AMD64,
          },
    },
  };
  static const ttx_schema observation = {
    8,
    8,
    TTX_SCHEMA_CALLABLE,
    {
      .callable =
          {
            arguments,
            1,
            {
              &integer,
              0,
            },
            TTX_SCHEMA_SYSTEM_V_AMD64,
          },
    },
  };
  static const ttx_schema_position fields[] = {
    {
      {
        NULL,
        TTX_SCHEMA_REFERENCE_POINTER,
      },
      offsetof(counter_api, context),
    },
    {
      {
        &addition,
        0,
      },
      offsetof(counter_api, add),
    },
    {
      {
        &observation,
        0,
      },
      offsetof(counter_api, read),
    },
  };
  static const ttx_schema schema = {
    sizeof(counter_api),
    _Alignof(counter_api),
    TTX_SCHEMA_COMPOSITE,
    {
      .composite =
          {
            fields,
            3,
          },
    },
  };
  compile_representation = compiler;
  if (!representation) {
    representation = prepare_representation(&schema);
  }

  reset(TTX_BINDING_SATISFIED, 0, 0);
  return (counter_fixture){
    {
      &state,
      bind,
      supports,
    },
    reset,
    statistics,
    compose,
  };
}
