// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_NEGOTIATION_QUERY_H
#define TTX_SEMANTIC_NEGOTIATION_QUERY_H

#include "perimortem/core/identity/uuid.h"

#include "ttx/data/form/storage.h"
#include "ttx/semantic/negotiation/status.h"

// Query negotiates semantic contracts before a consumer enters an Abstract
// graph. The requested UUID selects meaning, while destination Storage
// describes the exact API record the consumer can use, including function
// signatures and calling conventions. A module can publish the initial Query
// as described data, and another negotiated interface can supply later Queries.
// The supplying contract establishes this Query's own record representation and
// calling convention before either operation is called. A module bootstrap
// does that by transferring its described export into the consumer's known
// Query representation.
//
// `supports` reports Satisfied when the provider establishes the UUID's
// promise, Unknown when it leaves that promise undetermined, and Rejected when
// it explicitly refuses it. This checks meaning independently of
// representation. A consumer obtains callable operations through `bind`, which
// can be called directly with the required UUID and Storage.
//
// `bind` agrees the provider's API representation with the destination before
// transferring the complete record. The consumer uses that record only after
// Satisfied. Unknown and Rejected leave the destination unusable even if the
// provider changed its bytes. Both operations finish synchronously. A later
// call makes a new request under the provider's policy. Each status
// completes that call. Obtaining another result requires another request.
//
// The provider can construct the record, reuse stored functions with a suitable
// context, or delegate production. Each supplied function interprets its own
// context. The returned context can differ from the Query's context. Consumers
// call the supplied functions with their supplied context. Inspecting its
// private layout through Query is undefined behavior.
//
// The consumer keeps the complete negotiated record. Its declared members
// provide access to any embedded interfaces. Knowledge of an embedded interface
// establishes that member's layout, while using its enclosing record requires
// agreement on the enclosing representation.
//
// The caller keeps the destination writable until `bind` returns. The supplying
// contracts keep referenced state, descriptions and executable code accessible
// through every permitted use. Copying an API record preserves that access
// duration. Retention, release, invalidation and reload belong to the
// interfaces that explicitly provide those guarantees.
typedef struct ttx_semantic_query {
  void* context;
  ttx_negotiation_status (
      *bind)(void* context, perimortem_uuid contract, ttx_storage requested);
  ttx_negotiation_status (*supports)(void* context, perimortem_uuid contract);
} ttx_semantic_query;

C_LINKAGE EXPORTED(TTX) const ttx_representation*
    ttx_semantic_query_representation(void);

#ifdef __cplusplus

#include "perimortem/core/identity/uuid.hpp"
#include "perimortem/core/storage/result.hpp"

namespace Ttx::Semantic::Negotiation {

// Query calls the provider through its negotiated C API record. Typed binding
// supplies storage for the requested API and returns its C++ interface after
// successful negotiation. An interface can provide an additional `accept`
// check for the returned record. Representation and semantic agreement remain
// the same as for a caller that supplies Storage directly.
//
// The supplying contract keeps context and functions available through use.
// Each operation returns Rejected if its required function is absent.
class Query {
 public:
  constexpr Query() = default;

  // Copies the C record without calling its functions or checking its
  // representation. The caller establishes that representation first. Explicit
  // construction marks this unchecked conversion at the call site.
  constexpr explicit Query(ttx_semantic_query api) : api(api) {}

  constexpr operator ttx_semantic_query() const { return api; }

  // Reports whether both operation pointers are present. Default construction
  // leaves both absent. Pointer presence alone establishes neither their
  // calling convention nor the provider's semantic guarantees.
  constexpr auto is_set() const -> Bool {
    return api.bind != nullptr && api.supports != nullptr;
  }

  EXPORTED(TTX) auto supports(Perimortem::Core::Identity::Uuid contract) const
      -> Status;

  template <typename Contract>
  auto supports() const -> Status {
    return supports(Contract::contract_id);
  }

  EXPORTED(TTX) auto bind(
      Perimortem::Core::Identity::Uuid contract,
      Data::Form::Storage requested) const -> Status;

  template <typename Contract>
  auto bind() const -> Perimortem::Core::Storage::Result<Contract, Status> {
    return bind<Contract>(Contract::contract_id);
  }

  // The UUID selects the semantic contract, while Interface::Api describes
  // the requested record. A provider can support that UUID in several record
  // formats and selects the one matching the supplied Storage.
  //
  // After Satisfied, `Interface::accept(api)` runs when that operation exists.
  // A false result returns Rejected. Otherwise construction receives the
  // complete API record. The provider establishes its semantic guarantees
  // regardless of whether the C++ interface supplies this additional check.
  template <typename Interface>
  auto bind(Perimortem::Core::Identity::Uuid contract) const
      -> Perimortem::Core::Storage::Result<Interface, Status> {
    typename Interface::Api api = {};
    const auto requested = Data::Form::Storage::from(api);
    const auto status = bind(contract, requested);
    if (status != Status::Satisfied) {
      return status;
    }

    if constexpr (requires { Interface::accept(api); }) {
      if (!Interface::accept(api)) {
        return Status::Rejected;
      }
    }

    return Interface(api);
  }

 private:
  ttx_semantic_query api = ttx_semantic_query();
};

}  // namespace Ttx::Semantic::Negotiation

TTX_DATA_RECORD(
    perimortem_uuid,
    TTX_DATA_MEMBER(perimortem_uuid, high),
    TTX_DATA_MEMBER(perimortem_uuid, low));
TTX_DATA_RECORD(
    ttx_semantic_query,
    TTX_DATA_MEMBER(ttx_semantic_query, context),
    TTX_DATA_MEMBER(ttx_semantic_query, bind),
    TTX_DATA_MEMBER(ttx_semantic_query, supports));

#endif

#endif
