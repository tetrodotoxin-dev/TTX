// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/graph/capabilities/normalize.hpp"

#include "toolchain/validation/test.hpp"

#include "ttx/data/transfer.hpp"
#include "ttx/graph/policies/alias.hpp"
#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.hpp"
#include "ttx/graph/projection.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;
using namespace Ttx::Data;

static Harness Normalization = {.name = "TTX::Normalize"};

struct Slot {
  ttx_alias_slot storage;
  Abstract location = Policies::Alias::initialize(storage);

  auto redirect(Abstract target) -> Negotiation::Status {
    return location.bind<Policies::Alias>().visit(
        [&](Policies::Alias api) { return api.set_referent(target); },
        [] { return Negotiation::Status::Rejected; });
  }
};

static auto bind_normalize(Abstract location, ttx_normalize& output)
    -> Negotiation::Status {
  const auto api = location.get_abi();
  return Negotiation::Status(api.operations->bind(
      api.context, Capabilities::Normalize::contract_id, &output));
}

// The export has an independent Normalizer. Counting binds, support probes
// and calls distinguishes a failed acquisition from a failed resolve, and
// reveals a caller that checks support before trying to bind. The delegate
// can lead back into an Alias during the same observation.
struct Export
    : Projection<Export, 0x6cd6460b83f048b9ULL, 0x864bc0711b89b17bULL> {
  ttx_abstract target = ttx_unknown();
  ttx_normalize delegate{};
  Negotiation::Status acquisition = Negotiation::Status::Satisfied;
  Negotiation::Status resolution = Negotiation::Status::Satisfied;
  Count probes = 0;
  Count binds = 0;
  Count calls = 0;
  Bool incomplete_binding = False;

  auto supports_contract(Identity::Uuid id) -> Negotiation::Status {
    ++probes;
    if (id == Capabilities::Normalize::contract_id &&
        acquisition == Negotiation::Status::Rejected) {
      return Negotiation::Status::Rejected;
    }

    return Negotiation::Status::Unknown;
  }

  auto bind_contract(Identity::Uuid id, void* output) -> Negotiation::Status {
    if (id != Capabilities::Normalize::contract_id) {
      return Negotiation::Status::Rejected;
    }

    ++binds;
    if (acquisition == Negotiation::Status::Satisfied) {
      static constexpr ttx_normalize_ops operations = {resolve};
      *Data::cast<ttx_normalize>(output) = {
        this, incomplete_binding ? nullptr : &operations};
    }

    return acquisition;
  }

  static auto resolve(void* context, ttx_abstract* output)
      -> ttx_negotiation_status {
    auto& self = *Data::cast<Export>(context);
    ++self.calls;
    if (self.resolution != Negotiation::Status::Satisfied) {
      return ttx_negotiation_status(self.resolution);
    }

    if (self.delegate.operations) {
      return self.delegate.operations->resolve(self.delegate.context, output);
    }

    *output = self.target;
    return TTX_NEGOTIATION_SATISFIED;
  }
};

VALIDATION_TEST(Normalization, retain_links_and_resolve_again) {
  Slot a, b, c;
  ASSERT(a.redirect(b.location) == Negotiation::Status::Satisfied);
  ASSERT(b.redirect(c.location) == Negotiation::Status::Satisfied);

  Export definition;
  definition.acquisition = Negotiation::Status::Rejected;

  Export exported;
  exported.target = definition.get_abstract().get_abi();
  ASSERT(c.redirect(exported.get_abstract()) == Negotiation::Status::Satisfied);

  ttx_normalize acquired{};
  ASSERT(
      bind_normalize(a.location, acquired) == Negotiation::Status::Satisfied);

  ttx_normalize copy{};
  ASSERT(
      Transfer::copy(
          Form::Storage::from(acquired), Form::Storage::from(copy)) ==
      Status::Success);

  const Capabilities::Normalize normalizer(copy);
  ttx_abstract first{};
  ASSERT_EQ(
      copy.operations->resolve(copy.context, &first),
      TTX_NEGOTIATION_SATISFIED);
  EXPECT(Abstract(first) == definition.get_abstract());

  ttx_normalize at_definition{};
  EXPECT(
      Abstract(first).supports<Capabilities::Normalize>() ==
      Negotiation::Status::Rejected);
  EXPECT(
      bind_normalize(Abstract(first), at_definition) ==
      Negotiation::Status::Rejected);
  EXPECT_EQ(definition.calls, Count(0));
  EXPECT_EQ(exported.probes, Count(0));
  EXPECT_EQ(exported.binds, Count(1));
  EXPECT_EQ(exported.calls, Count(1));

  a.location.bind<Policies::Alias>().visit(
      [&](Policies::Alias alias) {
        EXPECT(alias.get_referent() == b.location);
      },

      [&] { EXPECT(False); });
  ASSERT(b.redirect(Abstract(ttx_none())) == Negotiation::Status::Satisfied);
  normalizer.resolve().visit(
      [&](Abstract now) {
        EXPECT(
            now.supports<Policies::None>() == Negotiation::Status::Satisfied);
      },

      [&](Negotiation::Status) { EXPECT(False); });
  EXPECT(Abstract(first) == definition.get_abstract());
  EXPECT_EQ(exported.calls, Count(1));
}

// Capture one Alias link before two redirects. The captured location remains
// usable and acquires a new subject that the original slot never denoted.
VALIDATION_TEST(Normalization, captured_link_across_redirects) {
  Slot a, b;
  const Abstract x(ttx_unknown());
  const Abstract c(ttx_none());
  Export exported;
  Export z;
  z.acquisition = Negotiation::Status::Rejected;
  exported.target = z.get_abstract().get_abi();
  ASSERT(b.redirect(x) == Negotiation::Status::Satisfied);
  ASSERT(a.redirect(b.location) == Negotiation::Status::Satisfied);

  const auto captured = a.location.bind<Policies::Alias>().visit(
      [](Policies::Alias link) { return link.get_referent(); },
      [&] {
        EXPECT(False);
        return a.location;
      });

  ASSERT(captured == b.location);

  ttx_normalize at_a{}, at_b{};
  ASSERT(bind_normalize(a.location, at_a) == Negotiation::Status::Satisfied);
  ASSERT(bind_normalize(captured, at_b) == Negotiation::Status::Satisfied);

  const auto expect_subject = [&](ttx_normalize api, Abstract expected) {
    Capabilities::Normalize(api).resolve().visit(
        [&](Abstract subject) { EXPECT(subject == expected); },
        [&](Negotiation::Status) { EXPECT(False); });
  };

  expect_subject(at_a, x);
  ASSERT(a.redirect(c) == Negotiation::Status::Satisfied);
  expect_subject(at_a, c);
  ASSERT(b.redirect(exported.get_abstract()) == Negotiation::Status::Satisfied);
  expect_subject(at_a, c);
  expect_subject(at_b, z.get_abstract());
}

VALIDATION_TEST(Normalization, independent_bind_and_operation_outcomes) {
  Slot slot;
  Export exported;
  ASSERT(
      slot.redirect(exported.get_abstract()) == Negotiation::Status::Satisfied);

  ttx_normalize normalizer{};
  EXPECT(
      slot.location.supports<Capabilities::Normalize>() ==
      Negotiation::Status::Satisfied);
  ASSERT(
      bind_normalize(slot.location, normalizer) ==
      Negotiation::Status::Satisfied);
  EXPECT_EQ(exported.probes, Count(0));
  EXPECT_EQ(exported.binds, Count(0));
  EXPECT_EQ(exported.calls, Count(0));

  const Negotiation::Status failures[] = {
    Negotiation::Status::Unknown, Negotiation::Status::Rejected};

  for (const Negotiation::Status failure : failures) {
    exported.acquisition = failure;

    ttx_abstract output{};
    EXPECT_EQ(
        normalizer.operations->resolve(normalizer.context, &output),
        ttx_negotiation_status(failure));
    EXPECT_EQ(exported.calls, Count(0));
  }

  exported.acquisition = Negotiation::Status::Satisfied;
  for (const Negotiation::Status failure : failures) {
    exported.resolution = failure;
    Capabilities::Normalize(normalizer)
        .resolve()
        .visit(
            [&](Abstract) { EXPECT(False); },
            [&](Negotiation::Status received) { EXPECT(received == failure); });
  }

  EXPECT_EQ(exported.calls, Count(2));

  ttx_abstract output{};
  exported.resolution = Negotiation::Status::Satisfied;
  exported.target = {};
  EXPECT_EQ(
      normalizer.operations->resolve(normalizer.context, &output),
      TTX_NEGOTIATION_REJECTED);
  exported.incomplete_binding = True;
  EXPECT_EQ(
      normalizer.operations->resolve(normalizer.context, &output),
      TTX_NEGOTIATION_REJECTED);
  EXPECT_EQ(exported.calls, Count(3));
}

// A delegated success must establish a different final subject both for the
// export that supplied it and for the slot from which the consumer started.
VALIDATION_TEST(Normalization, delegated_identity_is_not_a_normalization) {
  Slot slot;
  Export exported;
  ASSERT(
      slot.redirect(exported.get_abstract()) == Negotiation::Status::Satisfied);

  ttx_normalize api{};
  ASSERT(bind_normalize(slot.location, api) == Negotiation::Status::Satisfied);

  ttx_abstract output{};
  exported.target = exported.get_abstract().get_abi();
  EXPECT_EQ(
      api.operations->resolve(api.context, &output), TTX_NEGOTIATION_REJECTED);
  exported.target = slot.location.get_abi();
  EXPECT_EQ(
      api.operations->resolve(api.context, &output), TTX_NEGOTIATION_REJECTED);
  exported.target = ttx_none();
  ASSERT_EQ(
      api.operations->resolve(api.context, &output), TTX_NEGOTIATION_SATISFIED);
  EXPECT(
      Abstract(output).supports<Policies::None>() ==
      Negotiation::Status::Satisfied);
}

VALIDATION_TEST(Normalization, cycles_and_long_chains) {
  Slot chain[129];
  for (Count i = 1; i < 129; ++i) {
    ASSERT(
        chain[i - 1].redirect(chain[i].location) ==
        Negotiation::Status::Satisfied);
  }

  ttx_normalize api{};
  ASSERT(
      bind_normalize(chain[0].location, api) == Negotiation::Status::Satisfied);

  ttx_abstract output{};
  ASSERT_EQ(
      api.operations->resolve(api.context, &output), TTX_NEGOTIATION_SATISFIED);
  EXPECT(
      Abstract(output).supports<Policies::Unknown>() ==
      Negotiation::Status::Satisfied);
  ASSERT(
      chain[128].redirect(chain[120].location) ==
      Negotiation::Status::Satisfied);

  ttx_normalize fresh{};
  EXPECT(
      chain[0].location.supports<Capabilities::Normalize>() ==
      Negotiation::Status::Satisfied);
  ASSERT(
      bind_normalize(chain[0].location, fresh) ==
      Negotiation::Status::Satisfied);
  EXPECT_EQ(
      fresh.operations->resolve(fresh.context, &output),
      TTX_NEGOTIATION_UNKNOWN);
  EXPECT_EQ(
      api.operations->resolve(api.context, &output), TTX_NEGOTIATION_UNKNOWN);
  ASSERT(
      chain[0].redirect(chain[0].location) == Negotiation::Status::Satisfied);
  EXPECT(
      chain[0].location.supports<Capabilities::Normalize>() ==
      Negotiation::Status::Rejected);

  EXPECT(
      bind_normalize(chain[0].location, fresh) ==
      Negotiation::Status::Rejected);
  EXPECT_EQ(
      api.operations->resolve(api.context, &output), TTX_NEGOTIATION_REJECTED);

  // Reenter the chain through another provider's Normalize operation. A
  // later call after the cycle has been removed must reach the export again.
  Export exported;
  exported.delegate = api;
  ASSERT(
      chain[0].redirect(exported.get_abstract()) ==
      Negotiation::Status::Satisfied);
  EXPECT_EQ(
      api.operations->resolve(api.context, &output), TTX_NEGOTIATION_UNKNOWN);
  EXPECT_EQ(exported.calls, Count(1));
  exported.delegate = {};
  ASSERT(
      bind_normalize(chain[0].location, fresh) ==
      Negotiation::Status::Satisfied);
  ASSERT_EQ(
      api.operations->resolve(api.context, &output), TTX_NEGOTIATION_SATISFIED);
  EXPECT_EQ(exported.calls, Count(2));
  EXPECT(
      Abstract(output).supports<Policies::Unknown>() ==
      Negotiation::Status::Satisfied);
}

// Publish the same reserved policy from another context and table. The Alias
// must return this provider's record once support establishes its finality.
VALIDATION_TEST(Normalization, foreign_closed_policies) {
  static const ttx_abstract_ops view = {
    [](void* context, perimortem_uuid id) {
      const auto& source = *Data::cast<ttx_abstract>(context);
      return source.operations->supports(source.context, id);
    },

    [](void* context, perimortem_uuid id, void* output) {
      const auto& source = *Data::cast<ttx_abstract>(context);
      return source.operations->bind(source.context, id, output);
    },

    [](void* context, perimortem_view_bytes route) {
      const auto& source = *Data::cast<ttx_abstract>(context);
      return source.operations->resolve_route(source.context, route);
    },

    [](void* context, ttx_graph_visitor visitor) {
      const auto& source = *Data::cast<ttx_abstract>(context);
      source.operations->visit_routes(source.context, visitor);
    },

    [](void* context) {
      const auto& source = *Data::cast<ttx_abstract>(context);
      return source.operations->get_reachability(source.context);
    }};

  Slot slot;
  ttx_normalize normalizer{};
  ASSERT(
      bind_normalize(slot.location, normalizer) ==
      Negotiation::Status::Satisfied);

  ttx_abstract subjects[] = {ttx_none(), ttx_unknown()};
  for (auto& subject : subjects) {
    const Abstract presentation(&subject, view);
    ASSERT(slot.redirect(presentation) == Negotiation::Status::Satisfied);

    ttx_abstract captured{};
    ASSERT_EQ(
        normalizer.operations->resolve(normalizer.context, &captured),
        TTX_NEGOTIATION_SATISFIED);
    EXPECT(Abstract(captured) == presentation);
  }
}
