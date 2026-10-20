// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/transfer.hpp"
#include "ttx/semantic/negotiation/query.hpp"

#if __has_include("ttx/graph/abstract.h")
#error The Semantic component exposed Graph.
#endif

using namespace Perimortem::Core;
using namespace Perimortem::Serialization;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

struct Reading {
  void* context;
  U32 (*read)(void* context);
};

TTX_DATA_RECORD(
    Reading,
    TTX_DATA_MEMBER(Reading, context),
    TTX_DATA_MEMBER(Reading, read));

static constexpr Formats::Uuid contract(0x41ea753933a846cc, 0xba63a4301103d22e);

static auto read(void* context) -> U32 {
  return *Data::cast<U32>(context);
}

static auto supports(void* context, perimortem_uuid id)
    -> ttx_negotiation_status {
  return Formats::Uuid(id) == contract ? TTX_NEGOTIATION_SATISFIED
                                       : TTX_NEGOTIATION_UNKNOWN;
}

static auto bind(void* context, perimortem_uuid id, ttx_storage requested)
    -> ttx_negotiation_status {
  if (supports(context, id) != TTX_NEGOTIATION_SATISFIED) {
    return TTX_NEGOTIATION_UNKNOWN;
  }

  Reading api{context, read};
  return Transfer::copy(Form::Storage::from(api), Form::Storage(requested)) ==
                 Status::Success
             ? TTX_NEGOTIATION_SATISFIED
             : TTX_NEGOTIATION_REJECTED;
}

S32 main() {
  U32 value = 73;
  const Negotiation::Query query({&value, bind, supports});
  Reading api{};
  if (query.bind(contract, Form::Storage::from(api)) !=
      Negotiation::Status::Satisfied) {
    return 1;
  }

  return api.read(api.context) == value ? 0 : 2;
}
