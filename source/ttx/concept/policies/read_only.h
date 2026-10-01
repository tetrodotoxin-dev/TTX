// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/abstract.h"

#define TTX_READ_ONLY_ID_HIGH 0x2fea778924d84f91ULL
#define TTX_READ_ONLY_ID_LOW 0x88e6c246edcbf9d7ULL

// ReadOnly grants observation of its subject through the supplied view. Its
// operations leave that subject unchanged. A provider may return an existing
// conforming view or one exposing a smaller complete API. Support means the
// provider can supply this view.
//
// Further bindings, resolution and routes to the same subject preserve this
// access, as does acquiring a retained view. Reapplying ReadOnly preserves the
// same restrictions. Access to related subjects follows the policies their
// providers expose.
//
// A separate writable view may still change the subject, so observations can
// see new data. Constant separately governs the stability of a semantic answer.
// Each consumer keeps the access granted through its other views.
//
// The bound API is ttx_abstract, described by `ttx_abstract_representation`.
// Copies share the supplying observation's lifetime. Longer retention requires
// its own contract, and a Borrowed answer carries its release obligation.
// Successful binding supplies a complete API whose operations and exposed
// payloads honor ReadOnly.
