// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/providers/promise.hpp"

#include "perimortem/core/view/bytes.hpp"

#include "ttx/graph/policies/promise.hpp"
#include "ttx/graph/policies/unknown.hpp"
#include "ttx/graph/providers/resolution.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;

static auto target(void* context) -> ttx_abstract;

// A foreign implementation can forward a request back to the Promise that
// sent it. Keep the outstanding operation and its argument until delivery.
// Repeating that forwarding hop yields the pending response. Separate calls
// on the same handle can proceed when the outer observer receives a callback.
struct PromiseForward {
  enum class Operation { Support, Bind, Route, Visit, Reachability };

  void* origin;
  Operation operation;
  Identity::Uuid contract;
  View::Bytes name;
  PromiseForward* previous;
  inline static thread_local PromiseForward* active = nullptr;

  PromiseForward(
      void* origin,
      Operation operation,
      Identity::Uuid contract = {},
      View::Bytes name = {})
      : origin(origin),
        operation(operation),
        contract(contract),
        name(name),
        previous(active) {
    active = this;
  }

  ~PromiseForward() { active = previous; }

  auto subject() const -> ttx_abstract {
    for (auto* call = previous; call; call = call->previous) {
      if (call->origin == origin && call->operation == operation &&
          call->contract == contract && call->name == name) {
        return {};
      }
    }

    return target(origin);
  }
};

// A delivered visitor starts a new observation even when it visits the same
// Promise again. Restore the suspended forwarding chain before returning
// to the provider that is still enumerating routes.
static auto deliver(
    void* context,
    perimortem_view_bytes name,
    ttx_abstract value) -> void {
  struct Delivery {
    PromiseForward* suspended = PromiseForward::active;
    Delivery() { PromiseForward::active = nullptr; }
    ~Delivery() { PromiseForward::active = suspended; }
  } delivery;

  const auto& visitor = *Data::cast<ttx_graph_visitor>(context);
  visitor.callback(visitor.callback_context, name, value);
}

static auto supports(void* context, perimortem_uuid id) -> Negotiation::Status {
  PromiseForward call(
      context, PromiseForward::Operation::Support, Identity::Uuid(id));
  const auto selected = call.subject();
  if (selected.operations) {
    return selected.operations->supports(selected.context, id);
  }

  return Identity::Uuid(id) == Policies::Promise::contract_id
             ? Negotiation::Status::Satisfied
             : Negotiation::Status::Unknown;
}

static auto bind(void* context, perimortem_uuid id, void* output)
    -> Negotiation::Status {
  PromiseForward call(
      context, PromiseForward::Operation::Bind, Identity::Uuid(id));
  const auto selected = call.subject();
  if (selected.operations) {
    return selected.operations->bind(selected.context, id, output);
  }

  return Negotiation::Status::Unknown;
}

static auto route(void* context, perimortem_view_bytes name) -> ttx_abstract {
  PromiseForward call(
      context, PromiseForward::Operation::Route, {}, View::Bytes(name));
  const auto selected = call.subject();
  if (selected.operations) {
    return selected.operations->resolve_route(selected.context, name);
  }

  return ttx_unknown();
}

static auto visit(void* context, ttx_graph_visitor visitor) -> void {
  PromiseForward call(context, PromiseForward::Operation::Visit);
  const auto selected = call.subject();
  if (selected.operations) {
    selected.operations->visit_routes(selected.context, {&visitor, deliver});
  }
}

static auto reachability(void* context) -> ttx_abstract_reachability {
  PromiseForward call(context, PromiseForward::Operation::Reachability);
  const auto selected = call.subject();
  if (selected.operations) {
    return selected.operations->get_reachability(selected.context);
  }

  return TTX_ABSTRACT_INDETERMINATE;
}

static constexpr ttx_abstract_ops operations = {
  supports, bind, route, visit, reachability, Providers::Resolution::resolve};

static auto selection(void* context) -> ttx_abstract {
  return Providers::Resolution::follow(
      {context, &operations}, [](ttx_abstract link) {
        return link.operations == &operations &&
               Data::cast<ttx_abstract>(link.context)->operations;
      });
}

static auto target(void* context) -> ttx_abstract {
  const auto current = selection(context);
  return current.operations == &operations ? ttx_abstract{} : current;
}

auto Providers::Promise::get_abstract() & -> Abstract {
  const auto selected = selection(&subject);
  if (selected.operations) {
    return selected;
  }

  return Abstract(&subject, operations);
}

auto Providers::Promise::fulfill(Abstract candidate) -> Negotiation::Status {
  const ttx_abstract value = candidate;
  if (!Resolution::complete(value)) {
    return Negotiation::Status::Rejected;
  }

  // A foreign support operation can synchronously fulfill this Promise.
  // Recheck the commitment after that observation before publishing a value.
  const auto unresolved =
      !subject.operations &&
      candidate.supports<Policies::Unknown>() == Negotiation::Status::Satisfied;
  if (subject.operations) {
    return Abstract(subject) == candidate ? Negotiation::Status::Satisfied
                                          : Negotiation::Status::Rejected;
  }

  if (unresolved) {
    return Negotiation::Status::Unknown;
  }

  subject = value;
  return Negotiation::Status::Satisfied;
}
