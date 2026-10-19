// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_STATUS_H
#define TTX_DATA_STATUS_H

#include "toolchain/toolchain.h"

// Data operations report compilation, representation checks, traversal and
// transfer outcomes with these values. Each operation defines the failure
// conditions it can report and when its output becomes usable. Semantic
// negotiation reports its own Satisfied, Unknown and Rejected outcomes.
typedef U8 ttx_data_status;

#define TTX_DATA_SUCCESS ((ttx_data_status)0)
#define TTX_DATA_INVALID ((ttx_data_status)1)
#define TTX_DATA_BOUNDS ((ttx_data_status)2)
#define TTX_DATA_OVERFLOW ((ttx_data_status)3)
#define TTX_DATA_INCOMPATIBLE ((ttx_data_status)4)
#define TTX_DATA_UNSUPPORTED ((ttx_data_status)5)
#define TTX_DATA_DENIED ((ttx_data_status)6)

#endif
