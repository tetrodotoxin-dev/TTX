// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/algorithm/search.hpp"

#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/abstract.hpp"
#include "ttx/concept/policies/unknown.hpp"

#if __has_include(                         \
    "perimortem/serialization/png.hpp") || \
    __has_include("perimortem/compression/deflate.hpp")
#error TTX received an undeclared Perimortem component.
#endif

using namespace Toolchain::Validation;

static Harness SdkComponents = {.name = "Ttx::SdkComponents"};

VALIDATION_TEST(SdkComponents, core_runtime) {
  const U8 source[] = {'t', 't', 'x', ':', 'c', 'o', 'r', 'e'};
  const U8 value[] = {'c', 'o', 'r', 'e'};
  EXPECT_EQ(Perimortem::Core::Algorithm::search(source, value), 4u);
}

VALIDATION_TEST(SdkComponents, unknown_policy) {
  const auto value = Ttx::Concept::Policies::Unknown::get_unknown();
  EXPECT_EQ(
      value.supports<Ttx::Concept::Policies::Unknown>(),
      Ttx::Semantic::Negotiation::Binding::Status::Satisfied);
}
