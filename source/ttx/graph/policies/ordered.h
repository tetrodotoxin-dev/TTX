// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_POLICIES_ORDERED_H
#define TTX_GRAPH_POLICIES_ORDERED_H

#include "ttx/graph/abstract.h"

// Ordered gives meaning to the sequence produced by Abstract visitation.
// Repeated visits to unchanged provider state preserve that sequence. Changes
// in order represent changes in the provider's model. Repeated Abstracts
// remain separate occurrences, so consumers preserve their positions.
//
// Unknown leaves the ordering promise undetermined. Rejected establishes that
// order carries no meaning, including through the other policies supplied by
// this Abstract. An identical sequence across visits can still be incidental.
//
// Binding supplies the governing Abstract whose visitation has these rules.
// The visited Abstracts keep the access guarantees of their own contracts.
#define TTX_ORDERED_ID_HIGH 0x2726a909643e439dULL
#define TTX_ORDERED_ID_LOW 0x91059fe227c4c4edULL

#ifdef __cplusplus

namespace Ttx::Graph::Policies {

// Ordered carries the Abstract whose visitation order has semantic meaning.
// The consumer preserves repeated occurrences and their positions. Unknown
// leaves the ordering promise undetermined, while Rejected establishes that
// visitation order is incidental under every policy of that Abstract.
class Ordered {
 public:
  static constexpr auto contract_id =
      Perimortem::Core::Identity::Uuid(TTX_ORDERED_ID_HIGH, TTX_ORDERED_ID_LOW);
  using Api = ttx_abstract;

  // Copies an Abstract already supplied under the Ordered contract. Explicit
  // construction makes the caller's interpretation visible. Construction
  // performs a record copy, with the policy guaranteed by its provider.
  explicit constexpr Ordered(Api api) : api(api) {}

  constexpr auto get_abstract() const -> Abstract { return api; }

 private:
  Api api;
};

}  // namespace Ttx::Graph::Policies

#endif

#endif
