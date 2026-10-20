// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/algorithm/search.hpp"

#include "ttx/graph/abstract.hpp"
#include "ttx/graph/policies/unknown.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Semantic;

#if __has_include("perimortem/compression/deflate.hpp")
#error TTX received an undeclared Perimortem component.
#endif

static Harness SdkComponents = {.name = "Ttx::SdkComponents"};

VALIDATION_TEST(SdkComponents, core_runtime) {
  const U8 source[] = {'t', 't', 'x', ':', 'c', 'o', 'r', 'e'};
  const U8 value[] = {'c', 'o', 'r', 'e'};
  EXPECT_EQ(Algorithm::Search::match(source, value), Count(4));
}

VALIDATION_TEST(SdkComponents, unknown_policy) {
  const auto value = Policies::Unknown::get_unknown().get_abstract();
  EXPECT_EQ(
      value.supports<Policies::Unknown>(), Negotiation::Status::Satisfied);
}
