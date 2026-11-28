// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/graph/policies/alias.hpp"
#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.hpp"
#include "ttx/graph/projection.hpp"
#include "ttx/graph/providers/alias.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;
using Location = Abstract;

static Harness Resolution = {.name = "TTX::Resolve"};

VALIDATION_TEST(Resolution, retired_contracts) {
  const Location none = ttx_none();
  constexpr Identity::Uuid former_absence(0, 0);
  constexpr Identity::Uuid former_abstract(
      0x914c3769ef564732ULL, 0xa89c2453ec3f8636ULL);
  EXPECT(none.supports(former_absence) == Negotiation::Status::Rejected);

  const ttx_abstract record = none;
  ttx_abstract output{};
  EXPECT(
      record.operations->bind(record.context, former_abstract, &output) ==
      Negotiation::Status::Rejected);
}

struct TypeName
    : Projection<TypeName, 0xbcdbf34f21ab43afULL, 0xae68ca0d845726dcULL> {
  Location underlying;
  Location comment;
  Count resolutions = 0;

  auto resolve_route(View::Bytes name) const -> Location {
    if (name == "comment"_view) {
      return comment;
    }

    return ttx_none();
  }

  auto resolve() -> Location {
    ++resolutions;
    return underlying.resolve();
  }
};

VALIDATION_TEST(Resolution, local_documentation_and_foreign_reentry) {
  const Location none = ttx_none();
  const Location unknown = ttx_unknown();
  EXPECT(none.resolve() == none);
  EXPECT(unknown.resolve() == unknown);

  TypeName name{{}, none, unknown};
  Providers::Alias alias(name.get_abstract());
  const auto spot = alias.get_abstract();
  EXPECT(spot.resolve_route("comment"_view) == none);
  EXPECT(alias.get_referent().resolve_route("comment"_view) == unknown);
  EXPECT(spot.resolve() == none);
  EXPECT_EQ(name.resolutions, Count(1));
  EXPECT(alias.get_referent() == name.get_abstract());

  name.underlying = spot;
  EXPECT(spot.resolve() == unknown);
  EXPECT_EQ(name.resolutions, Count(2));
  name.underlying = none;
  EXPECT(spot.resolve() == none);
}

VALIDATION_TEST(Resolution, chains_cycles_and_recovery) {
  Providers::Alias links[1025];
  const Location none = ttx_none();
  const Location unknown = ttx_unknown();
  for (Count i = 1; i < 1025; ++i) {
    ASSERT(
        links[i - 1].set_referent(links[i]) == Negotiation::Status::Satisfied);
  }

  const Location start = links[0];
  EXPECT(start.resolve() == unknown);
  ASSERT(links[1024].set_referent(none) == Negotiation::Status::Satisfied);
  EXPECT(start.resolve() == none);
  ASSERT(
      links[1024].set_referent(links[300]) == Negotiation::Status::Satisfied);
  EXPECT(start.resolve() == unknown);
  EXPECT(links[1024].get_referent() == links[300].get_abstract());
  ASSERT(links[0].set_referent(start) == Negotiation::Status::Satisfied);
  EXPECT(start.resolve() == unknown);
  EXPECT(links[0].get_referent() == start);
  ASSERT(links[0].set_referent(none) == Negotiation::Status::Satisfied);
  EXPECT(start.resolve() == none);
}
