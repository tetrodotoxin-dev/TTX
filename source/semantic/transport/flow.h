// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_TRANSPORT_FLOW_H
#define TTX_SEMANTIC_TRANSPORT_FLOW_H

#include "ttx/data/form/representation.h"
#include "ttx/semantic/negotiation/query.h"

// Each pair names the consumer and provider promises independently of their
// Data API representations. All consumers use ttx_consumer, while each
// provider exposes the operations required by its transport.
#define TTX_DIRECT_CONSUMER_ID_HIGH 0xa425802b357f4d43ULL
#define TTX_DIRECT_CONSUMER_ID_LOW 0x8964f98d440dc3f1ULL
#define TTX_DIRECT_PROVIDER_ID_HIGH 0xbbac249cbf6f4feaULL
#define TTX_DIRECT_PROVIDER_ID_LOW 0x9f380bdbad5506f0ULL

#define TTX_SHARED_CONSUMER_ID_HIGH 0x5dbc7f1ffd8f4f58ULL
#define TTX_SHARED_CONSUMER_ID_LOW 0xa3ffbe6b0eb7ba8bULL
#define TTX_SHARED_PROVIDER_ID_HIGH 0x5e08a4e542e54709ULL
#define TTX_SHARED_PROVIDER_ID_LOW 0x975e1e3a8b82c574ULL

#define TTX_BLOCK_CONSUMER_ID_HIGH 0x52bac7f81dc34bc1ULL
#define TTX_BLOCK_CONSUMER_ID_LOW 0x93541f27c5ad1700ULL
#define TTX_BLOCK_PROVIDER_ID_HIGH 0xedd72b0997e14517ULL
#define TTX_BLOCK_PROVIDER_ID_LOW 0xad1b963e96c175b3ULL

#define TTX_FRAGMENT_CONSUMER_ID_HIGH 0xedd744c1533e4b1dULL
#define TTX_FRAGMENT_CONSUMER_ID_LOW 0x821d3753670c098aULL
#define TTX_FRAGMENT_PROVIDER_ID_HIGH 0x59e378fefd0c43f7ULL
#define TTX_FRAGMENT_PROVIDER_ID_LOW 0xadafab76bab6c547ULL

typedef U8 ttx_flow_status;
#define TTX_FLOW_REJECTED ((ttx_flow_status)32)
#define TTX_FLOW_UNKNOWN ((ttx_flow_status)33)

// Flow establishes one synchronous access agreement between already
// bootstrapped Queries. Direct, Shared, Block and Fragment are independent
// contracts tried in that preference order. The first compatible pair wins,
// and the host retains only that protocol's state for later operations.
//
// The consumer needs no target Storage during establishment. Each operation can
// supply its own storage without negotiating again. The opaque handle keeps
// the host's local protocol union out of the foreign module's C
// representation.
typedef struct ttx_flow ttx_flow;

// Knowing the required representation is enough to establish access. This
// consumer policy accepts all four Data protocols while operations supply their
// own storage later. The query and its Flows borrow the representation, so its
// owner keeps that description alive through every use of the agreement.
PERIMORTEM_C ttx_semantic_query
    ttx_flow_consumer(const ttx_representation* representation);

// The host supplies a fresh or closed Flow. Success means its representation
// or operations are ready before this call returns. Shared additionally lends
// one lifetime that remains acquired until Flow closes.
//
// Unknown means no candidate established an agreement. Flow may try another
// protocol after an Unknown binding or incompatible payload, but an explicit
// rejection stops that search. No work continues after return.
//
// Once a protocol is selected, acquisition reports Data operation failures
// unchanged. Even Data's Unsupported does not reopen negotiation after that
// selection. Endpoint state, representations and implementation code remain
// borrowed throughout subsequent Flow use.
PERIMORTEM_C ttx_flow_status ttx_flow_connect(
    ttx_flow* flow,
    ttx_semantic_query consumer,
    ttx_semantic_query provider);

#endif
