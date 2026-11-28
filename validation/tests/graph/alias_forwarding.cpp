// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/graph/policies/constant.hpp"
#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.hpp"
#include "ttx/graph/projection.hpp"
#include "validation/tests/graph/fixtures/observation.h"
#include "validation/tests/graph/fixtures/value.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;
using Location = Abstract;

static Harness AliasForwarding = {.name = "TTX::AliasForwarding"};

struct ModuleRevision
    : Projection<ModuleRevision, 0xb3cfe4bb28294c0eULL, 0x919dc6329da7129eULL> {
  const Location widget;

  auto resolve_route(View::Bytes name) const -> Location {
    if (name == "Widget"_view) {
      return widget;
    }

    return ttx_none();
  }
};

// ModuleImport forwards resolve_route and bind_contract to the published
// ModuleRevision. CommitOnce fixes the revision after the first publication.
// Redirect permits another publication at the same graph location. The owner
// publishes between observations and keeps each revision alive through use.
class ModuleImport : public Projection<
                         ModuleImport,
                         0x360a2d413a574a83ULL,
                         0x904e383616bd0263ULL> {
 public:
  enum class Updates { CommitOnce, Redirect };

  // An implicit conversion from Updates would create a fresh import each
  // time the owner passed a policy. Constructing ModuleImport explicitly
  // gives the build a particular location to retain across lookups.
  explicit ModuleImport(Updates updates) : updates(updates) {}

  auto publish(ModuleRevision& next) -> Bool {
    if (updates == Updates::CommitOnce && revision && revision != &next) {
      return False;
    }

    revision = &next;
    return True;
  }

  auto resolve_route(View::Bytes name) const -> Location {
    return target().resolve_route(name);
  }

  auto bind_contract(Identity::Uuid id, void* output) const
      -> Negotiation::Status {
    const ttx_abstract captured = target();
    return captured.operations->bind(captured.context, id, output);
  }

 private:
  auto target() const -> Location {
    if (revision) {
      return revision->get_abstract();
    }

    return ttx_unknown();
  }

  const Updates updates;
  ModuleRevision* revision = nullptr;
};

static auto lookup_widget(Location module) -> Location {
  return module.resolve_route("Widget"_view);
}

VALIDATION_TEST(AliasForwarding, one_commit_per_generation) {
  observation_subject definition{};
  definition.value = 41;
  definition.answer = TTX_NEGOTIATION_SATISFIED;

  ModuleRevision before{{}, ttx_none()};
  ModuleRevision after{{}, observation_abstract(&definition)};
  ModuleImport build(ModuleImport::Updates::CommitOnce);
  const auto old_import = build.get_abstract();
  EXPECT(
      lookup_widget(old_import).supports<Policies::Unknown>() ==
      Negotiation::Status::Satisfied);

  ASSERT(build.publish(before));
  EXPECT(
      lookup_widget(old_import).supports<Policies::None>() ==
      Negotiation::Status::Satisfied);
  EXPECT(build.publish(before));
  EXPECT_NOT(build.publish(after));

  ModuleImport edit(ModuleImport::Updates::CommitOnce);
  const auto new_import = edit.get_abstract();
  const auto pending = lookup_widget(new_import);
  EXPECT(
      pending.supports<Policies::Unknown>() == Negotiation::Status::Satisfied);
  ASSERT(edit.publish(after));

  const auto widget = lookup_widget(new_import);
  EXPECT(widget == lookup_widget(after.get_abstract()));
  widget.bind<Validation::Value>().visit(
      [&](Validation::Value api) { EXPECT_EQ(api.read(), U8(42)); },
      [&] { EXPECT(False); });
  EXPECT(
      pending.supports<Policies::Unknown>() == Negotiation::Status::Satisfied);
  EXPECT(
      lookup_widget(old_import).supports<Policies::None>() ==
      Negotiation::Status::Satisfied);
}

// A live import can report absence before an edit and select a declaration
// afterward. A reader that captured the earlier revision still sees absence.
VALIDATION_TEST(AliasForwarding, redirect_after_observed_absence) {
  observation_subject definition{};
  ModuleRevision before{{}, ttx_none()};
  ModuleRevision after{{}, observation_abstract(&definition)};
  ModuleImport import(ModuleImport::Updates::Redirect);
  const auto live = import.get_abstract();
  ASSERT(import.publish(before));

  const auto cached = lookup_widget(live);
  ASSERT(cached.supports<Policies::None>() == Negotiation::Status::Satisfied);
  EXPECT(
      cached.supports<Policies::Constant>() == Negotiation::Status::Rejected);

  auto pinned = live.bind<ModuleRevision::Abstract>();
  ASSERT(pinned);

  ASSERT(import.publish(after));

  const auto fresh = lookup_widget(live);
  EXPECT(fresh == after.widget);
  EXPECT(fresh.get_reachability() == Location::Reachability::Reachable);
  EXPECT(fresh.supports<Policies::None>() == Negotiation::Status::Rejected);
  pinned.visit(
      [&](ModuleRevision::Abstract grant) {
        EXPECT(grant.get_abstract() == before.get_abstract());
        EXPECT(lookup_widget(grant.get_abstract()) == cached);
      },

      [&] { EXPECT(False); });
}
