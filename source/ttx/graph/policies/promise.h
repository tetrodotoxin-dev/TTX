// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_PROMISE_H
#define TTX_GRAPH_POLICIES_PROMISE_H

// A provider reporting Satisfied for supports(Promise) owes a disposition.
// The obligation can remain pending indefinitely. While pending, the subject
// is Indeterminate, resolves to Unknown, and returns Unknown for bind requests.
// Other support requests return Unknown. Ordinary routes yield Unknown and
// visitation delivers zero callbacks.
//
// Once fulfilled, every observation goes to the chosen subject using its
// context and operations. If that subject also has an outstanding obligation,
// its Promise support is visible through this relationship. An unsuccessful
// support request supplies no evidence of fulfillment. Consumers can request
// their desired contract directly before or after the owner publishes a
// subject.
#define TTX_PROMISE_ID_HIGH 0x15b34b7dd7a3484aULL
#define TTX_PROMISE_ID_LOW 0xa690dc618b7af5caULL

#endif
