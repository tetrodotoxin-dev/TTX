// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/support/library.hpp"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/diagnostics/log.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/concept/abstract.hpp"
#include "ttx/data/transfer.hpp"
#include "ttx/semantic/negotiation/library.h"
#include "validation/providers/abstract/provider.h"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Perimortem::Serialization;
using namespace Perimortem::System;
using namespace Toolchain::Validation;
using namespace Ttx::Concept;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

TTX_DATA_RECORD(
    bootstrap_exports,
    TTX_DATA_MEMBER(bootstrap_exports, context),
    TTX_DATA_MEMBER(bootstrap_exports, visit));
TTX_DATA_RECORD(
    bootstrap_indexed,
    TTX_DATA_MEMBER(bootstrap_indexed, context),
    TTX_DATA_MEMBER(bootstrap_indexed, count),
    TTX_DATA_MEMBER(bootstrap_indexed, get));

static Harness NativeEntry = {.name = "TTX::Bootstrap"};

// This is the public bootstrap sequence. The export is described data. The
// first provider operation is called only after Transfer establishes the
// exported Query's representation and copies its record.
VALIDATION_TEST(NativeEntry, storage_to_query_to_abstract) {
  Allocator::Arena errors;
  const auto path =
      Validation::Support::library_path("libabstract_subject.so"_view);
  auto fail = [](View::Bytes message) { Diagnostics::Log::fatal(message); };

  Process::Library::open(path.get_view(), errors)
      .visit(
          [&](Process::Library& library) {
            library.symbol("ttx_entry"_view, errors)
                .visit(
                    [&](void* symbol) {
                      const auto& published =
                          *Data::cast<const ttx_storage>(symbol);
                      ttx_semantic_query entry{};
                      ttx_storage bootstrap{
                        ttx_semantic_query_representation(),
                        Data::cast<U8>(&entry), sizeof(entry)};

                      ASSERT_EQ(
                          ttx_transfer(&published, &bootstrap),
                          TTX_DATA_SUCCESS);
                      ASSERT(entry.bind && entry.supports);

                      ttx_abstract api{};
                      ttx_storage requested{
                        ttx_abstract_representation(), Data::cast<U8>(&api),
                        sizeof(api)};

                      const perimortem_uuid id{
                        TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW};

                      ASSERT_EQ(
                          entry.bind(entry.context, id, requested),
                          TTX_NEGOTIATION_SATISFIED);
                      ASSERT(Abstract::accept(api));
                      EXPECT(api.context != entry.context);

                      const auto bytes = api.operations->get_data(api.context);
                      EXPECT(View::Bytes(bytes) == "hello"_view);
                    },

                    fail);
          },

          fail);
}

// The export inventory's UUID accepts both callback and indexed APIs. Another
// UUID returns a Query with a different context. Each path reaches the same
// Abstract, establishing that semantic negotiation supports several entry APIs
// and chooses their representation independently of their contract UUID.
VALIDATION_TEST(NativeEntry, independent_paths_and_formats) {
  auto library =
      Validation::Support::open_library("libabstract_subject.so"_view);
  const auto* published = Data::cast<const ttx_storage>(
      Validation::Support::find_symbol(library, "ttx_entry"_view));
  ttx_semantic_query entry{};
  const auto target = Form::Storage::from(entry).get_abi();
  ASSERT_EQ(ttx_transfer(published, &target), TTX_DATA_SUCCESS);

  const Negotiation::Query query(entry);
  const Formats::Uuid exports(BOOTSTRAP_EXPORTS_HIGH, BOOTSTRAP_EXPORTS_LOW);
  EXPECT(query.supports(exports) == Negotiation::Status::Satisfied);

  bootstrap_exports callback_api{};
  ASSERT(
      query.bind(exports, Form::Storage::from(callback_api)) ==
      Negotiation::Status::Satisfied);

  Count visits = 0;
  callback_api.visit(
      callback_api.context, &visits, [](void* context, ttx_abstract api) {
        auto& count = *Data::cast<Count>(context);
        if (Abstract(api).get_data() == "hello"_view) {
          ++count;
        }
      });

  EXPECT_EQ(visits, Count(1));

  bootstrap_indexed indexed{};
  ASSERT(
      query.bind(exports, Form::Storage::from(indexed)) ==
      Negotiation::Status::Satisfied);
  ASSERT_EQ(indexed.count(indexed.context), U64(1));
  EXPECT(Abstract(indexed.get(indexed.context, 0)).get_data() == "hello"_view);

  ttx_semantic_query next{};
  ASSERT(
      query.bind(
          Formats::Uuid(BOOTSTRAP_QUERY_HIGH, BOOTSTRAP_QUERY_LOW),
          Form::Storage::from(next)) == Negotiation::Status::Satisfied);
  EXPECT(next.context != entry.context);
  Negotiation::Query(next).bind<Abstract>().visit(
      [&](Abstract api) { EXPECT(api.get_data() == "hello"_view); },

      [&](Negotiation::Status) { EXPECT(False); });

  ttx_abstract wrong{};
  EXPECT(
      query.bind(exports, Form::Storage::from(wrong)) ==
      Negotiation::Status::Rejected);
  EXPECT(
      query.bind(Formats::Uuid(0, 0), Form::Storage::from(wrong)) ==
      Negotiation::Status::Unknown);
  EXPECT(!wrong.context && !wrong.operations);
}

VALIDATION_TEST(NativeEntry, incompatible_bootstrap) {
  auto library =
      Validation::Support::open_library("libabstract_subject.so"_view);
  const auto* published = Data::cast<const ttx_storage>(
      Validation::Support::find_symbol(library, "ttx_entry"_view));
  ttx_abstract wrong{};
  const auto wrong_target = Form::Storage::from(wrong).get_abi();
  EXPECT_EQ(ttx_transfer(published, &wrong_target), TTX_DATA_INCOMPATIBLE);
  EXPECT(!wrong.context && !wrong.operations);

  ttx_semantic_query entry{};
  const auto target = Form::Storage::from(entry).get_abi();
  auto short_source = *published;
  short_source.size = sizeof(entry) - 1;
  EXPECT_EQ(ttx_transfer(&short_source, &target), TTX_DATA_BOUNDS);
  EXPECT(!entry.bind && !entry.supports && !entry.context);
}
