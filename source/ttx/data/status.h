// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_STATUS_H
#define TTX_DATA_STATUS_H

#include "toolchain/toolchain.h"

// Data operations return these status codes to report success or explain why
// they stopped. Each operation documents its possible failures and whether
// its output can be used after a failure. Semantic negotiation uses its own
// Satisfied, Unknown and Rejected results.
typedef U8 ttx_data_status;

#define TTX_DATA_SUCCESS ((ttx_data_status)0)
#define TTX_DATA_INVALID ((ttx_data_status)1)
#define TTX_DATA_BOUNDS ((ttx_data_status)2)
#define TTX_DATA_OVERFLOW ((ttx_data_status)3)
#define TTX_DATA_INCOMPATIBLE ((ttx_data_status)4)
#define TTX_DATA_UNSUPPORTED ((ttx_data_status)5)
#define TTX_DATA_DENIED ((ttx_data_status)6)

#endif
