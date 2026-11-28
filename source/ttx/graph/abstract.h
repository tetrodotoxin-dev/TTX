// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_GRAPH_ABSTRACT_H
#define TTX_GRAPH_ABSTRACT_H

#include "toolchain/toolchain.h"

#include "perimortem/core/identity/uuid.h"
#include "perimortem/core/view/bytes.h"

#include "ttx/data/form/representation.h"
#include "ttx/semantic/negotiation/status.h"

// The Abstract UUID identifies its complete callable ABI. Changing the
// published operations requires a new UUID for bootstrap negotiation.
#define TTX_ABSTRACT_ID_HIGH 0x0035c4e4db514293ULL
#define TTX_ABSTRACT_ID_LOW 0xa5d8d6be7b7bbd8cULL

// None and Unknown use reserved UUIDs in Graph. These identifiers describe the
// axiomatic graph results. The identifier for the former permanent absence
// policy (0, 0) is reserved. Other UUIDs identify independently chosen
// contracts.
#define TTX_NONE_ID_HIGH 0ULL
#define TTX_NONE_ID_LOW 2ULL
#define TTX_UNKNOWN_ID_HIGH 0ULL
#define TTX_UNKNOWN_ID_LOW 1ULL

// Reachability reports what this observation establishes about the selected
// target. Reachable supplies a graph location for the target. Unreachable
// establishes its absence for this selection. Indeterminate leaves its
// existence unsettled. The supplying contract determines how long the caller
// may rely on the observation.
//
// None establishes Unreachable for this selection.
// Unknown implies Indeterminate. A search provider can also report
// Indeterminate while exposing progress. A consumer must check support for
// the reserved policy before relying on its traversal or binding guarantees.
typedef U8 ttx_abstract_reachability;

#define TTX_ABSTRACT_UNREACHABLE ((ttx_abstract_reachability)0)
#define TTX_ABSTRACT_INDETERMINATE ((ttx_abstract_reachability)1)
#define TTX_ABSTRACT_REACHABLE ((ttx_abstract_reachability)2)

// Abstracts connect through route operations to form a graph independent of
// any host language's native type system. This lets providers and consumers
// navigate the same semantic relationships across languages and runtimes,
// then bind UUID contracts to obtain concrete APIs in their native type
// systems. TTX treats operations on those typed values as terminals that
// leave graph navigation and follow the bound contract.
//
// The Semantic Graph has two axiomatic results: None and Unknown. When a route
// resolves to None, the provider establishes absence for this lookup. A later
// lookup at the same origin may select an available target. Unknown
// leaves the route unsettled. The caller can try the original lookup again,
// but the provider makes no promise to schedule work or produce a target later.
//
// None and Unknown resolve to themselves, return themselves for every further
// route, and produce no
// ordinary visitation callbacks. This preserves the result without exposing
// where it entered a route chain. A reachable target can supply a location
// even when the consumer recognizes none of its application contracts.
//
// Policies and Capabilities provide two useful logical categories for Abstract
// contracts. Policy support establishes a property of the Abstract, while a
// Capability can be bound to obtain an extensible API surface. These categories
// organize contracts rather than restricting their design.
//
// None supports None and Abstract. A provider can independently promise
// Constant for the supplying edge. Unknown supports Unknown and Abstract. Only
// binding Abstract supplies its graph location. Every other support or bind
// request on Unknown returns the Unknown negotiation status.
//
// `supports` observes whether the provider establishes a contract's semantic
// promise. `bind` independently observes whether the provider can materialize
// the API selected by that contract UUID. Neither result implies the other. A
// supported contract can remain Unknown to `bind`, while a contract Unknown to
// `supports` can still bind successfully.
//
// Graph Unknown is an Abstract result. Negotiation Unknown leaves a support or
// binding request unsettled. A successful graph observation can return None or
// Unknown without making the observation itself a failure.
//
// Route observations stay within Graph, while binding enters typed operations.
//
//   Abstract --observation--> Abstract --bind(UUID)--> Type
//   Type     --terminal-----> Type
//   Type     --operation----> Abstract (any graph location)
//
// The graph provides transactional access through observations. Each Abstract
// API defines the results committed by one observation. An observation commits
// all of those results or none of them. A failed observation commits no
// callbacks, but the provider can still change graph state while attempting it.
// Retrying therefore _always_ begins a new transaction.
//
// When a route selects an Abstract supporting Constant, the caller can reuse
// that selection throughout the supplying access. Typed data and outgoing
// observations remain governed by their own contracts.
//
// `resolve_route` returns one observation directly. Ordinary visitation
// performs one observation for each visible route, so every callback can
// observe a different graph state. `Graph::Capabilities::Selection` can
// establish one observation over its complete callback set when the target
// Abstract supplies that capability.
//
// A successful bind supplies the complete requested contract, including every
// required operation and policy. The guarantee applies to that returned API.
// Each further binding is a separate observation whose success depends on the
// provider supplying that API. Some contracts require a particular binding,
// as None requires binding Abstract. Otherwise a provider can refuse later
// requests, including the bound contract's own UUID or Abstract, while the
// operations already supplied remain usable.
//
// A successful bind of Abstract returns the exact semantic location this
// node represents. Different API records can represent that same location.
// A typed operation can return an Abstract anywhere in the graph. If the
// typed API supplies `get_abstract`, it returns the location that granted
// that API throughout the caller's access.
//
// The supplying contract establishes the complete API representation before
// the consumer calls its operations. That representation includes the table's
// C calling conventions and recursive Abstract results. A usable Abstract has
// a nonnull context, a readable operation table and every operation populated.
// Each API is valid only as its published construction of context and
// operations. The supplied functions interpret that context's private layout.
// A provider can publish several interfaces over the same backing state with
// different contexts or operations. Consumers preserve each supplied pairing.
//
// The supplying contract defines how long the context and operation table
// remain accessible. Copies of the Abstract share that duration, and graph
// observations do not extend it. Stability follows the Policies supplied by
// that Abstract.
//
// A plain Abstract result requires no release from its consumer. Stateful
// resources cross the graph boundary through typed contracts that define their
// own access and release obligations.
typedef struct ttx_abstract {
  void* context;
  const struct ttx_abstract_ops* operations;
} ttx_abstract;

// Visitation invokes the consumer's callback for each visible route. Every
// callback is an independent observation of the Abstract selected for that
// route. The callback can make further graph observations before the provider
// continues, and provider state can change before the next callback. Consumers
// requiring one transaction across several results can bind Selection when the
// provider supplies it.
typedef struct ttx_graph_visitor {
  void* callback_context;
  void (*callback)(
      void* callback_context,
      perimortem_view_bytes route,
      ttx_abstract value);
} ttx_graph_visitor;

typedef struct ttx_abstract_ops {
  // Reports whether the provider establishes the UUID's semantic promise.
  // The provider returns Satisfied when it establishes that promise, Unknown
  // when it leaves the promise undetermined, and Rejected when it refuses it.
  // Obtaining the API requires a separate `bind` request, whose result can
  // differ as provider state changes. This call transfers no API record.
  //
  // Using a context pointer other than the pointer provided when `supports` was
  // acquired is undefined behavior.
  ttx_negotiation_status (*supports)(void* context, perimortem_uuid contract);

  // Supplies the complete C API record identified by the UUID. The caller
  // provides writable output storage of that contract's exact type, size and
  // alignment. The established Abstract ABI and UUID determine the record
  // layout and callable signatures. The provider may construct the record
  // from stored, generated or delegated state.
  //
  // The consumer uses the API after `bind` returns Satisfied. A call returning
  // Unknown or Rejected leaves the output unusable. The consumer controls
  // the output record, while each referenced context, description and
  // function follows its supplying access contract. Copying the record
  // preserves those access durations.
  //
  // Binding Abstract to this node returns this node's own semantic location
  // on Satisfied. Each subsequent bind is a new observation.
  //
  // The caller uses the context supplied with this operation. Substituting
  // another context through this interface is undefined behavior.
  ttx_negotiation_status (
      *bind)(void* context, perimortem_uuid contract, void* output);

  // Resolves a navigation request using this Abstract's policy. The provider
  // defines the route's byte format and supplies the complete returned
  // Abstract. A route describes this request, while an edge describes a graph
  // relationship. Further navigation uses the returned Abstract's operations.
  // Binding requests its typed contracts separately.
  ttx_abstract (*resolve_route)(void* context, perimortem_view_bytes route);

  // Visitation enumerates the routes visible through this Abstract's policy
  // and their Abstracts. Each callback preserves the traversed state until it
  // returns. Visitation order acquires meaning only through a separately
  // negotiated policy.
  void (*visit_routes)(void* context, ttx_graph_visitor visitor);

  // Reports reachability of the selected target in one observation. A provider
  // waiting for a target may report Indeterminate and let the caller bind a
  // progress interface on this Abstract. The caller uses the supplied context
  // and can observe the result without requesting either reserved policy.
  ttx_abstract_reachability (*get_reachability)(void* context);

  // Returns the subject this position denotes in one observation. An ordinary
  // position resolves to itself. An indirection can return a different subject
  // or Unknown when unresolved. A provider keeps its policy in force when
  // selecting the subject. With the same state, resolving a subject again
  // denotes the same semantic position. Each call receives the complete handle.
  // The caller can retain that subject through its grant. A subsequent named
  // route and any Constant promise on its result belong to that subject.
  // Another resolve at the original location is a fresh observation.
  ttx_abstract (*resolve)(ttx_abstract self);
} ttx_abstract_ops;

// An ordinary node can install `ttx_resolve_self` in its operation table.
// Returning the supplied record preserves its context and operations for the
// caller even when another provider shares the same callback.
static inline ttx_abstract ttx_resolve_self(ttx_abstract self) {
  return self;
}

C_LINKAGE
EXPORTED(TTX) const ttx_representation* ttx_abstract_representation(void);

#ifdef __cplusplus

#include "perimortem/core/data.hpp"
#include "perimortem/core/identity/uuid.hpp"
#include "perimortem/core/storage/option.hpp"
#include "perimortem/core/storage/vector.hpp"
#include "perimortem/core/view/bytes.hpp"

#include "ttx/graph/visitor.hpp"
#include "ttx/semantic/negotiation/query.h"

namespace Ttx::Graph {

class Abstract {
 public:
  enum class Reachability : U8 {
    Unreachable = TTX_ABSTRACT_UNREACHABLE,
    Indeterminate = TTX_ABSTRACT_INDETERMINATE,
    Reachable = TTX_ABSTRACT_REACHABLE,
  };

  static constexpr auto contract_id = Perimortem::Core::Identity::Uuid(
      TTX_ABSTRACT_ID_HIGH,
      TTX_ABSTRACT_ID_LOW);
  using Api = ttx_abstract;
  using Operations = ttx_abstract_ops;
  using Visitor = Graph::Visitor<Abstract>;

  // Stores the context pointer and the address of the operation table. The
  // caller supplies a complete Abstract interface and keeps its context and
  // table available for every use of this record.
  constexpr Abstract(void* context, const Operations& operations)
      : api(context, &operations) {}

  // The C and C++ handles copy the same context and operation table in either
  // direction. The caller supplies a complete record under the Abstract
  // contract. Both copies share the original access duration.
  constexpr Abstract(Api api) : api(api) {}
  constexpr operator Api() const { return api; }

  // Resolves this position without selecting a named route. The consumer may
  // retain the subject under its grant and make route observations there.
  auto resolve() const -> Abstract { return api.operations->resolve(api); }

  // Observes whether the selected target can be reached. The consumer can
  // ask before it knows any application contracts. A provider reporting
  // Indeterminate can still offer contracts explaining the pending result.
  auto get_reachability() const -> Reachability {
    return Reachability(api.operations->get_reachability(api.context));
  }

  auto supports(Perimortem::Core::Identity::Uuid contract) const
      -> Semantic::Negotiation::Status {
    return Semantic::Negotiation::Status(
        api.operations->supports(api.context, contract));
  }

  template <typename Contract>
  auto supports() const -> Semantic::Negotiation::Status {
    return supports(Contract::contract_id);
  }

  // A successful UUID request supplies the complete Contract::Api record.
  // Binding Abstract returns this node's own semantic location on success.
  // The empty Option represents either an undetermined or refused request.
  template <typename Contract>
  auto bind() const -> Perimortem::Core::Storage::Option<Contract> {
    typename Contract::Api output{};
    const auto status =
        api.operations->bind(api.context, Contract::contract_id, &output);
    if (status == TTX_NEGOTIATION_SATISFIED) {
      return Contract(output);
    }

    return {};
  }

  // Traverses a route through the provider's policy. The returned Abstract
  // supports continued traversal and UUID negotiation. The provider owns the
  // binary route format. A route can be a name or instructions for negotiation
  // between domains.
  //
  // None and Unknown return themselves for further routes. Chaining resolution
  // preserves either terminal result regardless of which step reached it.
  //
  // Each lookup observes the provider again. If the result supplies Constant,
  // the caller can reuse the selection for this Abstract and route during
  // the supplying access. None reports absence for this lookup.
  // A fresh lookup requires valid access to the same semantic origin.
  auto resolve_route(Perimortem::Core::View::Bytes route) const -> Abstract {
    return api.operations->resolve_route(
        api.context, perimortem_view_bytes(route.get_data(), route.get_size()));
  }

  // The provider advertises the routes visible through this Abstract's policy.
  // Callbacks let it walk its own representation without first constructing
  // and transferring a collection of API records, at the cost of a callback for
  // each advertised route.
  //
  // Each callback is an independent observation. It can make further graph
  // observations through the supplied Abstract, and those operations can
  // change provider state before the next callback. A consumer requiring one
  // transaction across several results can bind Selection when the provider
  // supplies it.
  //
  // Route bytes survive only their callback and must be copied if kept.
  // References encoded in those bytes follow the provider's route contract.
  auto visit_routes(Visitor visitor) const -> void {
    const ttx_graph_visitor callback(&visitor, callback_route);
    api.operations->visit_routes(api.context, callback);
  }

  // Compares the published context and table pointers. The `get_abstract` round
  // trip preserves semantic graph location, which can have different API
  // records.
  constexpr auto operator==(const Abstract& other) const -> Bool {
    return api.context == other.api.context &&
           api.operations == other.api.operations;
  }

  constexpr auto operator!=(const Abstract& other) const -> Bool {
    return !(*this == other);
  }

 private:
  // `visit_routes` supplies its local Visitor as callback context for the
  // complete synchronous enumeration.
  static auto callback_route(
      void* context,
      perimortem_view_bytes route,
      Api abstract) -> void {
    auto& visitor = *Perimortem::Core::Data::cast<Visitor>(context);
    visitor(Perimortem::Core::View::Bytes(route), Abstract(abstract));
  }

  Api api;
};

static_assert(sizeof(Abstract) == sizeof(ttx_abstract));

}  // namespace Ttx::Graph

TTX_DATA_RECORD(
    perimortem_view_bytes,
    TTX_DATA_MEMBER(perimortem_view_bytes, data),
    TTX_DATA_MEMBER(perimortem_view_bytes, size));

// Semantic bootstrap checks Abstract's complete C ABI against this description.
// Typed Graph bindings use the UUID and fixed C contract directly.
//
// Declare the root schema first so recursive signatures can refer back to it.
// Native reads argument and result types from the C fields and supplies their
// calling convention. The root's fields are defined after the operation and
// visitor schemas, once their references are available.
template <>
class Ttx::Data::Form::Native<ttx_abstract> {
  static_assert(
      __is_standard_layout(ttx_abstract) &&
          __is_trivially_constructible(ttx_abstract, const ttx_abstract&) &&
          __is_trivially_destructible(ttx_abstract),
      "Native API records require a trivial C-compatible call boundary");

 public:
  static const Schema schema;
  static constexpr Schema::Reference reference = schema;
};

TTX_DATA_RECORD(
    ttx_graph_visitor,
    TTX_DATA_MEMBER(ttx_graph_visitor, callback_context),
    TTX_DATA_MEMBER(ttx_graph_visitor, callback));

TTX_DATA_RECORD(
    ttx_abstract_ops,
    TTX_DATA_MEMBER(ttx_abstract_ops, supports),
    TTX_DATA_MEMBER(ttx_abstract_ops, bind),
    TTX_DATA_MEMBER(ttx_abstract_ops, resolve_route),
    TTX_DATA_MEMBER(ttx_abstract_ops, visit_routes),
    TTX_DATA_MEMBER(ttx_abstract_ops, get_reachability),
    TTX_DATA_MEMBER(ttx_abstract_ops, resolve));

inline constexpr Ttx::Data::Form::Schema
    Ttx::Data::Form::Native<ttx_abstract>::schema = [] {
      static constexpr Perimortem::Core::Storage::Vector<Schema::Position, 2>
          fields = {{
            TTX_DATA_MEMBER(ttx_abstract, context),
            TTX_DATA_MEMBER(ttx_abstract, operations),
          }};

      return Schema::composite(
          fields.get_view(), sizeof(ttx_abstract), alignof(ttx_abstract));
    }();

#endif

#endif
