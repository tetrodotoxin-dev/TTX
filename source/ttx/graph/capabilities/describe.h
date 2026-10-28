// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_CAPABILITIES_DESCRIBE_H
#define TTX_GRAPH_CAPABILITIES_DESCRIBE_H

#include "ttx/graph/abstract.h"

#define TTX_DESCRIBE_ID_HIGH 0x1214ecbcb2a24558ULL
#define TTX_DESCRIBE_ID_LOW 0xab4e23d85e9b88d5ULL

// Describe supplies human readable metadata for a graph location. Name is a
// label and description is documentation, both encoded as UTF 8 with explicit
// byte lengths. A consumer chooses which metadata its publication requires.
// Supplying Describe makes each getter available. Either value may be absent.
// Names carry no uniqueness, route-selection or native-type identity promise.
//
// A getter returns one after writing an available View, including an empty
// View, or zero when the metadata is absent. The caller supplies writable
// output storage and uses it only after one is returned. The provider keeps
// returned bytes readable and unchanged until the next getter through this
// published Describe API or the end of its supplying access, whichever comes
// first. Copies of that API share this validity window. A consumer copies text
// it needs beyond that window. A later getter may supply different metadata.
typedef struct ttx_describe {
  void* context;
  const struct ttx_describe_ops* operations;
} ttx_describe;

typedef struct ttx_describe_ops {
  // Returns to the graph location that supplied this binding, using the
  // provider's complete Abstract record and its own context and operations.
  ttx_abstract (*get_abstract)(void* context);
  U8 (*get_name)(void* context, perimortem_view_bytes* output);
  U8 (*get_description)(void* context, perimortem_view_bytes* output);
} ttx_describe_ops;

#endif
